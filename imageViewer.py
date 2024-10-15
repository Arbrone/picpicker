import sys
import os
import io
import rawpy
import glob
import numpy as np
import queue
from pathlib import Path
from PIL import Image
from PySide6.QtWidgets import (QApplication,
                               QPushButton,
                               QWidget,
                               QVBoxLayout, 
                               QGridLayout, 
                               QLabel, 
                               QScrollArea,
                               QProgressBar)
from PySide6.QtCore import (QObject,
                            Signal, 
                            Slot,
                            QRunnable,
                            QThreadPool)
from PySide6.QtGui import (QPixmap, 
                           QImage,
                           QShortcut,
                           QTransform)
from PySide6.QtCore import Qt


class ImageViewerWidget(QWidget):
    def __init__(self):
        super().__init__()

        #self.image_path = image_path
        self.photo_label = QLabel(self)
        self.photo_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # SHORTCUTS
        # self.next_image_shortcut = QShortcut(Qt.Key.Key_Right,self.photo_label)
        # self.next_image_shortcut.activated.connect(self.next_image)

        # self.prev_image_shortcut = QShortcut(Qt.Key.Key_Left,self.photo_label)
        # self.prev_image_shortcut.activated.connect(self.prev_image)

        self.rotate_shortcut = QShortcut(Qt.Key.Key_R, self.photo_label)
        self.rotate_shortcut.activated.connect(self.rotate_image)

    def show_image(self, image_path, width, height):
        pixmap = self.get_pixmap(image_path)
        pixmap = pixmap.scaled(width, height, Qt.KeepAspectRatio, Qt.SmoothTransformation)
        self.photo_label.setPixmap(pixmap)

    def get_pixmap(self, image_path):
        if image_path.endswith("RAF"):
            return self.get_pixmap_from_raw(image_path)
        return self.get_pixmap_from_compressed(image_path)
    
    def get_pixmap_from_raw(self, image_path):
        with rawpy.imread(image_path) as raw:
            rgb_image = raw.postprocess(half_size=True) # CORRECT

        height, width, _ = rgb_image.shape
        
        # Utilise PIL pour s'assurer que les images ont la bonne orientation
        pil_image = Image.fromarray(rgb_image)
        if pil_image.height > pil_image.width:
            pil_image = pil_image.rotate(270, expand=True)  # Corriger l'orientation si nécessaire
        
        # Convertir PIL image en QImage pour l'affichage
        data = pil_image.tobytes()
        width, height = pil_image.size
        q_image = QImage(data, width, height, QImage.Format_RGB888)
        pixmap = QPixmap.fromImage(q_image)        
        return pixmap

    def get_pixmap_from_compressed(self, image_path):
        pixmap = QPixmap(image_path)
        return pixmap

    Slot()
    def rotate_image(self):
        pixmap = self.photo_label.pixmap()

        if pixmap is not None:
            transform = QTransform()
            transform.rotate(90)

            rotated_pixmap = pixmap.transformed(transform, Qt.SmoothTransformation)
            scaled_pixmap = rotated_pixmap.scaled(self.photo_label.width(), self.photo_label.height(), Qt.KeepAspectRatio, Qt.SmoothTransformation)

            # Mettre à jour l'image dans le QLabel
            self.photo_label.setPixmap(scaled_pixmap)