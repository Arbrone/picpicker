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
                           QImage,)
from PySide6.QtCore import Qt

class ThumbailsWidget (QWidget):
    def __init__(self, images_path):
        super().__init__()
        self.images_path = images_path

        # Crée un layout principal avec juste une barre de progression
        self.layout = QVBoxLayout()

        # Widget contenant la grille de miniatures
        self.scroll_area = QScrollArea(self)
        self.scroll_area.setWidgetResizable(True)
        self.grid_widget = QWidget()
        self.grid_layout = QGridLayout(self.grid_widget)
        self.scroll_area.setWidget(self.grid_widget)

        # Ajouter la zone de défilement au layout principal
        self.layout.addWidget(self.scroll_area)

        # Crée une barre de progression
        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, len(self.images_path))  # Définir la valeur maximale
        self.layout.addWidget(self.progress_bar)  # Ajouter la barre de progression au layout principal


        self.setLayout(self.layout)

        self.threadpool = QThreadPool()
        self.data_queue = queue.Queue()

    # Function for the producer (loading data)
    def data_loader(self):
        for item in self.images_path:
            self.data_queue.put(item)  # Add the data to the queue
        self.data_queue.put(None)  # Signal the end of data loading

    def update_progress_bar(self):
        value = self.progress_bar.value() + 1
        self.progress_bar.setValue(value)

    @Slot(QPixmap)
    def fill_grid_thumbnails(self, thumbnail, image_path):
        thumbnail_label = QLabel(self)
        thumbnail_label.setObjectName(image_path)
        thumbnail_label.setPixmap(thumbnail.scaled(200, 200, Qt.KeepAspectRatio, Qt.SmoothTransformation))
        thumbnail_label.setScaledContents(False)
        # thumbnail_label.setAlignment(Qt.AlignmentFlag.AlignHCenter)
        # thumbnail_label.mousePressEvent = lambda event, path=image_path: self.show_image(path)

        # Ajouter la miniature dans la grille
        row, column = self.get_thumbnail_coord(image_path)
        self.grid_layout.addWidget(thumbnail_label, row, column)

    def get_thumbnail_coord(self, image_path):
        idx = self.images_path.index(image_path)
        return idx//6, idx%6

    def display_thumbnails(self):
        self.data_loader()

        # Supprimer les miniatures précédentes
        for i in reversed(range(self.grid_layout.count())):
            widget = self.grid_layout.itemAt(i).widget()
            if widget is not None:
                widget.deleteLater()

        image_path = self.data_queue.get()
        while image_path :
           worker = ThumbnailWorker(image_path)
           worker.signals.progress.connect(self.update_progress_bar)
           worker.signals.result.connect(self.fill_grid_thumbnails)
           self.threadpool.start(worker)
           image_path = self.data_queue.get()

class ThumbnailWorkerSignals(QObject):
    # Signal pour retourner les résultats (miniature prête à être affichée)
    result = Signal(QPixmap, str)
    progress = Signal()


class ThumbnailWorker(QRunnable):
    def __init__(self, image_path) -> None:
        super().__init__()
        self.image_path = image_path
        self.base_ext = ['png', 'jpg', 'jpeg', 'bmp']
        self.raw_ext = ['raf']
        self.signals = ThumbnailWorkerSignals()

    def run(self) -> None:
        print(f"Start : {Path(self.image_path).name}")
        image_format = self.image_path.lower().split('.')[1]
        if image_format in self.base_ext:
            thumbnail = self.get_thumbnail_from_compressed(self.image_path)
        elif image_format in self.raw_ext:
            thumbnail = self.get_thumbnail_from_raw(self.image_path)
        print(f"Done : {Path(self.image_path).name}")
        
        self.signals.result.emit(thumbnail, self.image_path)
        self.signals.progress.emit()
    
    def get_thumbnail_from_raw(self, image_path):
        with rawpy.imread(image_path) as raw:
            thumbnail = raw.extract_thumb()

        if thumbnail.format == rawpy.ThumbFormat.JPEG:
            binary_data = thumbnail.data
            image_stream = io.BytesIO(binary_data)
            image = Image.open(image_stream)
            rgb_matrix = np.array(image)
        elif thumbnail.format == rawpy.ThumbFormat.RAW:
            thumb_image = thumbnail.postprocess()
            rgb_matrix = np.array(thumb_image)

        q_image = QImage(rgb_matrix, rgb_matrix.data.shape[1], rgb_matrix.data.shape[0], QImage.Format_RGB888)
        return QPixmap.fromImage(q_image)


    def get_thumbnail_from_compressed(self, image_path):
        return QPixmap(image_path)