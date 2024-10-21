import json

import cv2
import pyttsx3
import requests

from ultralytics.models.yolo.model import YOLO

baseUrl = "http://192.168.4.1/"
urlSensor = f"{baseUrl}sensor"
urlSensorSamping = f"{baseUrl}sensorSamping"

def getDataSensor():
    try:
        response = requests.get(urlSensor)
        if response.status_code == 200:
            # Mengambil konten JSON dari respons
            data = response.json()
            
            # Akses data sesuai dengan kunci yang ada di JSON
            sensor_kiri = round(data.get("sensor_Kiri"), 0)
            sensor_tengah = round(data.get("sensor_Tengah"), 0)
            sensor_kanan = round(data.get("sensor_Kanan"), 0)
            
            gabunganSensor = f"{sensor_kiri};{sensor_tengah};{sensor_kanan}"
            return gabunganSensor
        else:
            print(f"Error: {response.status_code}")
            return None
        
    except requests.exceptions.RequestException as e:
        # Menangani kesalahan pada request
        print(f"Error: {e}")
        return None

# Load YOLOv8n model
model = YOLO('best.pt')

def get_object_position(x_bbox, width_frame):
    # Define the boundaries for left, center, and right
    batas_kiri = width_frame / 3
    batas_tengah = 2 * width_frame / 3
    
    if x_bbox <= batas_kiri:
        return "Kiri"
    elif batas_kiri < x_bbox <= batas_tengah:
        return "Tengah"
    else:
        return "Kanan"
    
# Open video capture from ESP32-CAM (replace with your ESP32-CAM IP)
esp32_cam_url = 'http://192.168.4.2'
cap = cv2.VideoCapture(esp32_cam_url)

# Set frame width and height (if necessary, but may not affect IP stream)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

# Define confidence threshold
confidence_threshold = 0.8

# Initialize pyttsx3 engine
engine = pyttsx3.init()

# Inisialisasi variabel untuk menyimpan status objek yang terdeteksi
detected_objects = set()

while True:
    # Capture frame from video
    ret, frame = cap.read()
    
    if not ret:
        print("Gagal menangkap frame dari ESP32-CAM")
        break

    # Get frame dimensions
    height_frame, width_frame = frame.shape[:2]
    
    # Perform object detection
    results = model(frame)
    
    current_detected_objects = set()  # Set untuk objek yang terdeteksi saat ini
    
    # Loop through each detection
    for box in results[0].boxes:  # Access the first frame's boxes
        # Get the bounding box coordinates
        x_min, y_min, x_max, y_max = box.xyxy[0].cpu().numpy()
        
        # Get the predicted class and confidence score
        class_id = int(box.cls[0].cpu().numpy())
        confidence = box.conf[0].cpu().numpy()
        
        # Check if confidence is above the threshold
        if confidence < confidence_threshold:
            continue  # Skip this detection if below threshold
        
        # Get the class name from the model
        class_name = model.names[class_id]
        
        # Calculate x_bbox (center of the detected bounding box)
        x_bbox = (x_min + x_max) / 2

        # Determine object position
        posisi_objek = get_object_position(x_bbox, width_frame)

        # Draw bounding box
        cv2.rectangle(frame, (int(x_min), int(y_min)), (int(x_max), int(y_max)), (0, 255, 0), 2)

        # Create label with class name, confidence, and position
        label = f'{class_name} ({confidence:.2f}) - {posisi_objek}'
        
        # Draw the label above the bounding box
        cv2.putText(frame, label, (int(x_min), int(y_min) - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 0, 255), 2)

        # Tambahkan objek yang terdeteksi ke dalam set
        current_detected_objects.add(class_name)

    # Cek apakah ada objek baru yang terdeteksi
    new_objects = current_detected_objects - detected_objects
    # Cek apakah ada objek yang hilang
    lost_objects = detected_objects - current_detected_objects

    # Jika ada objek baru, ucapkan namanya
    for obj in new_objects:
        engine.say(f"Objek terdeteksi: {obj}")
    
    # Jika ada objek yang hilang, ucapkan pesan bahwa objek telah hilang
    for obj in lost_objects:
        engine.say(f"Objek hilang: {obj}")
    
    # Jalankan pyttsx3 hanya jika ada yang terdeteksi atau hilang
    if new_objects or lost_objects:
        engine.runAndWait()

    # Update status objek yang terdeteksi
    detected_objects = current_detected_objects

    # Display the frame with detections
    cv2.imshow('YOLOv8 Object Detection on ESP32-CAM', frame)

    # Break loop with 'q'
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# Release video capture and close windows
cap.release()
cv2.destroyAllWindows()
