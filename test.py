import sys
import glob
import os
from thumbnail import ThumbailsWidget
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QApplication,
                               QMainWindow,
                               QHBoxLayout,
                               QVBoxLayout,
                               QPushButton,
                               QGroupBox,
                               QCheckBox, 
                               QWidget)

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.resize(1600, 900)

        folder_path = "/home/thomas/Workspace/picpicker/data/117_FUJI"
        images_path = sorted([file for file in glob.glob(folder_path + "/*") if os.path.isfile(file)])
        print(len(images_path))

        self.main_widget = QWidget()

        self.thumbnails_widget = ThumbailsWidget(images_path)

        # Layout vertical pour le panneau de gauche (GroupBox + Bouton Validate)
        self.left_panel = QVBoxLayout()
        self.button = QPushButton("Load images")
        self.button.clicked.connect(self.thumbnails_widget.display_thumbnails)
        self.left_panel.addWidget(self.button)

        self.main_layout = QHBoxLayout()
        self.main_layout.addLayout(self.left_panel, 2)  # Ajouter le panneau de gauche avec le groupbox et le bouton
        self.main_layout.addWidget(self.thumbnails_widget, 10) 
        self.main_widget.setLayout(self.main_layout)

        self.setCentralWidget(self.main_widget)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    startWidget = MainWindow()
    startWidget.show()
    sys.exit(app.exec())


