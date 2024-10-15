from pathlib import Path
import shutil
from PySide6.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QLineEdit, QPushButton, QLabel
from PySide6.QtCore import Slot

class ValidateSelectionWidget(QWidget):
    def __init__(self, folder_path, images_selection):
        super().__init__()
        self.selected_folder_path = Path(folder_path, "selected")
        self.rejected_folder_path = Path(folder_path, "rejected")
        self.images_selection = images_selection


        # Configuration de la fenêtre
        self.setWindowTitle("Validation")
        self.setGeometry(300, 300, 400, 150)

        # Layout principal vertical
        main_layout = QVBoxLayout()

        # ---- Section "select" ----
        select_layout = QHBoxLayout()

        # Label et QLineEdit pour "select"
        select_label = QLabel("Selected:")
        self.select_line_edit = QLineEdit()
        self.select_line_edit.setText(str(self.selected_folder_path))

        # Bouton pour changer de répertoire "select"
        self.select_button = QPushButton("Change")
        self.select_button.clicked.connect(lambda: self.set_new_directory("selected"))

        # Ajout au layout horizontal pour "select"
        select_layout.addWidget(select_label)
        select_layout.addWidget(self.select_line_edit)
        select_layout.addWidget(self.select_button)

        # ---- Section "rejected" ----
        rejected_layout = QHBoxLayout()

        # Label et QLineEdit pour "rejected"
        rejected_label = QLabel("Rejected:")
        self.rejected_line_edit = QLineEdit()
        self.rejected_line_edit.setText(str(self.rejected_folder_path))

        # Bouton pour changer de répertoire "rejected"
        self.rejected_button = QPushButton("Change")
        self.rejected_button.clicked.connect(lambda: self.set_new_directory("rejected"))

        # Ajout au layout horizontal pour "rejected"
        rejected_layout.addWidget(rejected_label)
        rejected_layout.addWidget(self.rejected_line_edit)
        rejected_layout.addWidget(self.rejected_button)

        # ---- Bouton de validation ----
        self.validate_button = QPushButton("Validate")
        self.validate_button.clicked.connect(self.validate_selection)

        # Ajouter les layouts au layout principal
        main_layout.addLayout(select_layout)
        main_layout.addLayout(rejected_layout)
        main_layout.addWidget(self.validate_button)

        # Appliquer la disposition à la fenêtre
        self.setLayout(main_layout)

    @Slot()
    def set_new_directory(self, selection:str):
        print("set_new_directory")
        new_folder_path = QFileDialog.getExistingDirectory(self, "Select Folder")
        if selection == "selected":
            self.selected_folder_path = Path(new_folder_path)
            line_edit = self.select_line_edit
        else:
            self.rejected_folder_path = Path(new_folder_path)        
            line_edit = self.rejected_line_edit

        line_edit.setText(new_folder_path)

        
    @Slot()
    def validate_selection(self):
        # Create directories for selected and rejected files
        self.selected_folder_path.mkdir(parents=True, exist_ok=True)
        self.rejected_folder_path.mkdir(parents=True, exist_ok=True)

        # Move selected files
        self.move_files(self.images_selection["selected"], self.selected_folder_path)
        # Move rejected files
        self.move_files(self.images_selection["rejected"], self.rejected_folder_path)
        self.close()
        
    def move_files(self, image_paths, destination_directory):
        for image_path in image_paths:
            source = Path(image_path)
            
            if source.exists():
                destination = destination_directory / source.name  # Using the `/` operator to create the path
                try:
                    shutil.move(str(source), str(destination))  # Move the file
                except Exception as e:
                    print(f"Error moving {source} to {destination}: {e}")
            else:
                print(f"Source file does not exist: {source}")
