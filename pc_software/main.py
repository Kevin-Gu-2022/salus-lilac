from recognition import detect_objects
from send_file import send_image

IMAGE_PATH = "soccer_ball.jpg"

def main():
    detect_objects(IMAGE_PATH)
    send_image("bounding_boxed.jpg")

if __name__ == "__main__":
    main()