import cv2
import base64
import numpy as np
import os

class VisionModule:
    def __init__(self, tests_dir=None):
        self.is_active = False
        self.tests_dir = tests_dir
        if not self.tests_dir:
             self.tests_dir = os.getcwd()

    def start(self):
        self.is_active = True

    def stop(self):
        self.is_active = False

    def process_image_data(self, image_data):
        """
        Takes raw JPEG bytes from ESP32.
        1. Decodes to CV2
        2. Rotates 180 degrees (Software correction)
        3. Applies filters (Denoise, Artifact removal)
        4. Saves debug
        5. Returns base64 for LLM
        """
        try:
            nparr = np.frombuffer(image_data, np.uint8)
            frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            
            if frame is None:
                print("Error: Received invalid image data from ESP32")
                return None
            
            # --- Software Pipeline ---
            
            # Rotation (180 degrees)
            # Corrects upside-down mounting
            frame = cv2.rotate(frame, cv2.ROTATE_180)
            
            # Median blur is excellent for "salt-and-pepper" noise and thin stripes
            frame = cv2.medianBlur(frame, 3)
            
            # Optional: Gaussian Blur for general smoothing if noisy
            # frame = cv2.GaussianBlur(frame, (3, 3), 0)
            
            # This brings out details in bad lighting
            lab = cv2.cvtColor(frame, cv2.COLOR_BGR2LAB)
            l, a, b = cv2.split(lab)
            clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8,8))
            cl = clahe.apply(l)
            limg = cv2.merge((cl,a,b))
            frame = cv2.cvtColor(limg, cv2.COLOR_LAB2BGR)
            
            # Save debug
            debug_path = os.path.join(self.tests_dir, "debug_vision_capture.jpg")
            cv2.imwrite(debug_path, frame)
            print(f"Debug: Saved processed frame (Rotated+Filtered) to {debug_path}")
            
            _, buffer = cv2.imencode('.jpg', frame, [int(cv2.IMWRITE_JPEG_QUALITY), 90])
            jpg_as_text = base64.b64encode(buffer).decode('utf-8')
            
            return jpg_as_text
            
        except Exception as e:
            print(f"Error processing image: {e}")
            return None

