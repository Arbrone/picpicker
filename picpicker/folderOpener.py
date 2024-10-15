import glob
import os
from PySide6.QtWidgets import (QWidget,
                               QFileDialog,
                               QVBoxLayout,
                               QPushButton,
                               QSizePolicy)
from PySide6.QtCore import (Qt,
                            QObject,
                            Slot,
                            Signal)

class FolderOpenerWidget(QWidget):
    def __init__(self, folder_path_callback):
        super().__init__()
        self.signals = FolderOpenerSignals()
        self.signals.result.connect(folder_path_callback)

        self.folder_layout = QVBoxLayout()
        self.folder_button = QPushButton("Open Folder")
        #self.folder_button.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        self.folder_button.clicked.connect(self.open_folder_dialog)
        self.folder_layout.addWidget(self.folder_button)
        self.setLayout(self.folder_layout)
    
    @Slot()
    def open_folder_dialog(self):
        # TODO enlever le chemin vers ./data
        folder_path = QFileDialog.getExistingDirectory(self, "Open Folder", "/home/thomas/Workspace/picpicker/data/save")
        self.signals.result.emit(folder_path)

class FolderOpenerSignals(QObject):
    result = Signal(str)