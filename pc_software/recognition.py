"""
A short script to detect objects using YOLOv8ls

"""
from ultralytics import YOLO

def detect_objects(image_path):
    # Saves a model called yolov8n.pt
    model = YOLO("yolov8n.pt")
    results = model(image_path)
    names = model.names

    # Get the boxes from the results list 
    for box in results[0].boxes:
        cls_id = int(box.cls[0])
        conf = box.conf[0].item()
        print(f"Detected: {names[cls_id]} with confidence {conf:.2f}")

if __name__ == "__main__":
    detect_objects("soccer_ball.jpg")