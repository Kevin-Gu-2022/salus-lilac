"""
A short script to detect objects using YOLOv8ls

"""
from ultralytics import YOLO
import cv2

def detect_objects(image_path):
    # Saves a model called yolov8n.pt
    model = YOLO("yolov8n.pt")
    model.fuse()  # Make faster
    results = model(image_path)
    names = model.names

    # Get the boxes from the results list 
    for box in results[0].boxes:
        cls_id = int(box.cls[0])
        conf = box.conf[0].item()
        label = f"{results[0].names[cls_id]}: {conf:.2f}"
        
        print(f"Detected: {label}")

        x1, y1, x2, y2 = map(int, box.xyxy[0])  # bounding box

        (label_width, label_height), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 2)
        y = max(y1, label_height + 10)  # ensure label is inside image

        # Draw rectangle and label
        cv2.rectangle(results[0].orig_img, (x1, y1), (x2, y2), (0, 255, 0), 2)
        cv2.putText(results[0].orig_img, label, (x1, y - 5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)

    # Save only once after drawing all boxes
    cv2.imwrite("bounding_boxed.jpg", results[0].orig_img)


if __name__ == "__main__":
    detect_objects("soccer_ball.jpg")