"""
A short script to detect objects using YOLOv8.

Takes one command line argument - the file path to image to perform recognition.

"""
from ultralytics import YOLO
import cv2
import sys
import os

def detect_objects(image_path):
    """
    Takes an image and performs recognition using the YOLOv8 model. Draws the
    bounding boxes and saves as "bounding_boxed.jpeg".
    """
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

    #  Save the image to out directory
    i = 0
    while os.path.exists(f"out/bounded_{i}.jpg"):
        i += 1

    filename = f"out/bounded_{i}.jpg"
    # Save only once after drawing all boxes
    cv2.imwrite(filename, results[0].orig_img)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        path = sys.argv[1].lower() # Get the first argument and convert to lowercase
        detect_objects(path)
    else:
        print("No path to image given")
        sys.exit(-1)
    