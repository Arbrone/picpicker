import sys
import os
import io
import rawpy
import glob
import numpy as np
import shutil
from pathlib import Path
from PIL import Image
from PySide6.QtWidgets import (QApplication,
                               QMainWindow, 
                               QWidget, 
                               QPushButton, 
                               QVBoxLayout, 
                               QHBoxLayout, 
                               QGridLayout, 
                               QLabel, 
                               QFileDialog,
                               QLineEdit,
                               QFormLayout, 
                               QStackedWidget, 
                               QScrollArea, 
                               QListWidget, 
                               QListWidgetItem, 
                               QCheckBox, 
                               QGroupBox,
                               QProgressBar)
from PySide6.QtCore import (Signal, 
                            Slot,
                            QRunnable,
                            Qth)
from PySide6.QtGui import (QPixmap, 
                           QImage, 
                           QScreen, 
                           QShortcut,
                           QTransform)
from PySide6.QtCore import Qt

class ValidateWindow(QWidget):
    """
    This "window" is a QWidget. If it has no parent, it
    will appear as a free-floating window as we want.
    """
    def __init__(self, images_selection:dict):
        super().__init__()
        layout = QVBoxLayout()
        self.description = QLineEdit()
        form_layout = QFormLayout()
        form_layout.addRow("Selected images destination", self.description)
        layout.addWidget(self.label)
        self.setLayout(layout)


class PicPicker(QMainWindow):
    
    def __init__(self):
        super().__init__()

        # self.folder_path = folder_path
        # self.images_path = [file for file in glob.glob(folder_path + "/*") if os.path.isfile(file)]
        # self.visible_images_path = sorted(self.images_path)
        self.images_selection = {"selected":[],
                                 "rejected":[]}
        self.current_image_index = 0
        self.prev_image_buffer = None
        self.next_image_buffer = None

        self.init_interface()
        # self.display_thumbnails()

    def init_interface(self):
        self.setWindowTitle("Photo Viewer")
        self.resize(1600, 900)

        self.validate_window = None

        # Créer un QStackedWidget pour gérer différents widgets (chargement et interface principale)
        self.stacked_widget = QStackedWidget(self)

        self.folder_widget = QWidget()
        self.folder_layout = QVBoxLayout()
        self.folder_button = QPushButton("Open folder")
        self.folder_button.clicked.connect(self.open_folder_dialog)
        self.folder_layout.addWidget(self.folder_button)
        self.folder_widget.setLayout(self.folder_layout)

        # Widget principal avec le layout de l'interface
        self.main_widget = QWidget()

        # Utiliser un QScrollArea pour afficher la grille avec un défilement
        self.scroll_area = QScrollArea(self)
        self.scroll_area.setWidgetResizable(True)

        # Widget pour la sélection des formats d'image (GroupBox pour les checkboxes)
        self.image_format_widget = QGroupBox("Formats")
        self.raw_checkbox = QCheckBox("RAF", self)
        self.jpg_checkbox = QCheckBox("JPG", self)

        # Définir les checkboxes à l'état "coché" par défaut
        self.raw_checkbox.setCheckState(Qt.CheckState.Checked)
        self.jpg_checkbox.setCheckState(Qt.CheckState.Checked)

        # Connexion des checkboxes aux signaux
        self.raw_checkbox.stateChanged.connect(lambda: self.set_visible_images_path("RAF"))
        self.jpg_checkbox.stateChanged.connect(lambda: self.set_visible_images_path("JPG"))

        # Layout pour les options de format d'image (checkboxes dans le groupbox)
        self.format_layout = QVBoxLayout()
        self.format_layout.addWidget(self.raw_checkbox)
        self.format_layout.addWidget(self.jpg_checkbox)
        self.image_format_widget.setLayout(self.format_layout)
        
        self.select_button = QPushButton("Select")
        self.reject_button = QPushButton("Reject")
        self.sort_button = QPushButton("Sort")
        self.validate_button = QPushButton("Validate")

        self.select_button.clicked.connect(self.select_image)
        self.reject_button.clicked.connect(self.reject_image)
        self.sort_button.clicked.connect(self.sort_thumbails)
        self.validate_button.clicked.connect(self.validate_selection)
        
        self.move_raw_jpg_checkbox = QCheckBox("auto select RAW + JPG", self)
        self.move_raw_jpg_checkbox.setCheckState(Qt.CheckState.Checked)

        self.selection_image_layout = QHBoxLayout()
        self.selection_image_layout.addWidget(self.select_button)
        self.selection_image_layout.addWidget(self.reject_button)
        #self.selection_image_layout.addWidget(self.validate_button)

        # Layout vertical pour le panneau de gauche (GroupBox + Bouton Validate)
        self.left_panel = QVBoxLayout()
        self.left_panel.addWidget(self.image_format_widget)  # Ajouter le groupbox des formats d'image
        self.left_panel.addStretch()  # Ajoute un espace flexible pour pousser le bouton en bas
        self.left_panel.addWidget(self.move_raw_jpg_checkbox)
        self.left_panel.addLayout(self.selection_image_layout)
        self.left_panel.addWidget(self.sort_button)
        self.left_panel.addStretch()
        self.left_panel.addWidget(self.validate_button)  # Ajouter le bouton validate en bas

        # Widget contenant la grille de miniatures
        self.grid_widget = QWidget()
        self.grid_layout = QGridLayout(self.grid_widget)
        self.grid_layout.setHorizontalSpacing(10)  # Espacement entre les colonnes
        self.grid_layout.setVerticalSpacing(20)
        self.grid_layout.setContentsMargins(10, 10, 10, 10)
        self.scroll_area.setWidget(self.grid_widget)

        # Widget pour afficher l'image en grand
        self.photo_label = QLabel(self)
        self.photo_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.photo_label.hide()

        # Layout principal de l'interface (layout horizontal avec panneau de gauche + scroll area + zone de prévisualisation)
        self.main_layout = QHBoxLayout()
        self.main_layout.addLayout(self.left_panel, 2)  # Ajouter le panneau de gauche avec le groupbox et le bouton
        self.main_layout.addWidget(self.scroll_area, 10)  # Ajouter la scroll area
        self.main_layout.addWidget(self.photo_label, 10)  # Ajouter le label pour afficher l'image sélectionnée

        # Encapsuler le layout principal dans un widget
        self.main_widget.setLayout(self.main_layout)

        # Widget et layout pour le chargement (avec barre de progression)
        self.loading_widget = QWidget()
        self.progress_bar = QProgressBar()
        self.loading_layout = QVBoxLayout()
        self.loading_layout.addWidget(self.progress_bar)

        # Encapsuler le layout de chargement dans un widget
        self.loading_widget.setLayout(self.loading_layout)

        # Ajouter les widgets (main_widget et loading_widget) au QStackedWidget
        self.stacked_widget.addWidget(self.folder_widget)
        self.stacked_widget.addWidget(self.loading_widget)  # Index 0 : Widget de chargement
        self.stacked_widget.addWidget(self.main_widget)  # Index 1 : Widget principal

        # Mettre le QStackedWidget dans le layout principal de l'application
        # main_container = QVBoxLayout(self)
        # main_container.addWidget(self.stacked_widget)
        self.setCentralWidget(self.stacked_widget)

        # Shortcuts
        self.next_image_shortcut = QShortcut(Qt.Key.Key_Right,self.photo_label)
        self.next_image_shortcut.activated.connect(self.next_image)

        self.prev_image_shortcut = QShortcut(Qt.Key.Key_Left,self.photo_label)
        self.prev_image_shortcut.activated.connect(self.prev_image)

        self.hide_shortcut = QShortcut(Qt.Key.Key_Escape, self.photo_label)
        self.hide_shortcut.activated.connect(self.hide_photo)

        self.select_shortcut = QShortcut(Qt.Key.Key_Up, self.photo_label)
        self.select_shortcut.activated.connect(self.select_image)
        self.reject_shortcut = QShortcut(Qt.Key.Key_Down, self.photo_label)
        self.reject_shortcut.activated.connect(self.reject_image)

        self.rotate_shortcut = QShortcut(Qt.Key.Key_R, self.photo_label)
        self.rotate_shortcut.activated.connect(self.rotate_image)

    # Check state of checkbox and set self.visible_images_path accordingly
    def set_visible_images_path(self, format):
        if format == "RAF" :
            is_check = self.raw_checkbox.isChecked()
        else :
            is_check = self.jpg_checkbox.isChecked()

        if is_check:
            images_path_to_add = [img_path for img_path in self.images_path if img_path.endswith(format)]
            self.visible_images_path = sorted(self.visible_images_path + images_path_to_add)
        else:
            self.visible_images_path = sorted([img_path for img_path in self.visible_images_path if not img_path.endswith(format)])

        self.display_thumbnails()

    def display_thumbnails(self):
        # Supprimer les miniatures précédentes
        for i in reversed(range(self.grid_layout.count())):
            widget = self.grid_layout.itemAt(i).widget()
            if widget is not None:
                widget.deleteLater()

        base_ext = ['png', 'jpg', 'jpeg', 'bmp']
        raw_ext = ['raf'] # TODO trouver la liste exhaustive des formats
        
        # Charger les images et afficher les miniatures
        # TODO ajouter un barre de chargement
        # passer par un autre thread
        self.progress_bar.setRange(0, len(self.visible_images_path))

        #self.file_path = sorted(os.listdir(folder_path))
        progress_bar_value = 0
        row, col = 0, 0
        for image_path in self.visible_images_path:
            image_format = image_path.lower().split('.')[1]
            if image_format in base_ext:
                thumbnail = self.get_thumbnail_from_compressed(image_path)
            elif image_format in raw_ext:
                thumbnail = self.get_thumbnail_from_raw(image_path)
            
            # Créer un QLabel pour afficher la miniature
            thumbnail_label = QLabel(self)
            thumbnail_label.setObjectName(image_path)
            thumbnail_label.setPixmap(thumbnail.scaled(200, 200, Qt.KeepAspectRatio, Qt.SmoothTransformation))
            # TODO mettre les images portrait en vertical dans la grille
            thumbnail_label.setScaledContents(False)
            #thumbnail_label.setAlignment(Qt.AlignmentFlag.AlignHCenter)
            # thumbnail_label.mouseDoubleClickEvent = lambda event, path=image_path: self.photo_double_clicked(path)
            thumbnail_label.mousePressEvent = lambda event, path=image_path: self.show_image(path)
            # Ajouter la miniature dans la grille
            self.grid_layout.addWidget(thumbnail_label, row, col)

            # Gérer la disposition en grille
            col += 1
            if col > 5:  # 6 miniatures par ligne
                col = 0
                row += 1
            
            progress_bar_value += 1
            self.progress_bar.setValue(progress_bar_value)
        self.progress_bar.reset()
        self.update_thumbnails_color()
        self.stacked_widget.setCurrentIndex(2)

    def get_thumbnail(self, image_path):
        if image_path.endswith("RAW"):
            thumbnail = self.get_thumbnail_from_raw(image_path)
        else:
            thumbnail = self.get_thumbnail_from_compressed(image_path)
        
        return thumbnail

    def get_thumbnail_from_raw(self, image_path):
            with rawpy.imread(image_path) as raw:
                thumbnail = raw.extract_thumb()

            if thumbnail.format == rawpy.ThumbFormat.JPEG:
                binary_data = thumbnail.data
                image_stream = io.BytesIO(binary_data)
                # Utiliser Pillow pour lire les données JPEG en tant qu'image
                image = Image.open(image_stream)
                rgb_matrix = np.array(image)
            elif thumbnail.format == rawpy.ThumbFormat.RAW:
                # Si la miniature est un RAW, convertir en RGB
                thumb_image = thumbnail.postprocess()
                # thumb_image est déjà une matrice RGB (numpy array)
                rgb_matrix = np.array(thumb_image)

            # Convertir l'image en QImage et la redimensionner
            q_image = QImage(rgb_matrix, rgb_matrix.data.shape[1], rgb_matrix.data.shape[0], QImage.Format_RGB888)
            #q_image = q_image.scaled(200, 200, Qt.KeepAspectRatio, Qt.SmoothTransformation)

            # Créer un QPixmap à partir de QImage et le retourner
            return QPixmap.fromImage(q_image)


    def get_thumbnail_from_compressed(self, image_path):
        return QPixmap(image_path)#.scaled(200, 200, Qt.KeepAspectRatio, Qt.SmoothTransformation)

    def show_image(self, image_path):
        self.scroll_area.hide()
        self.photo_label.show()

        pixmap = self.get_pixmap(image_path)
        self.photo_label.setPixmap(pixmap)

        self.current_image_index = self.visible_images_path.index(image_path)

        self.toggle_selection(image_path)

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
        
        pixmap = pixmap.scaled(self.photo_label.width(), self.photo_label.height(), Qt.KeepAspectRatio, Qt.SmoothTransformation)
        return pixmap

    def get_pixmap_from_compressed(self, image_path):
        pixmap = QPixmap(image_path)
        pixmap = pixmap.scaled(self.photo_label.width(), self.photo_label.height(), Qt.KeepAspectRatio, Qt.SmoothTransformation)
        return pixmap
    
    @Slot()
    def hide_photo(self):
        self.photo_label.hide()
        self.scroll_area.show()
        self.select_button.setStyleSheet("")
        self.reject_button.setStyleSheet("")
        self.update_thumbnails_color()

    @Slot()
    def next_image(self):
        if self.current_image_index+1 < len(self.visible_images_path):
            self.current_image_index += 1
            self.show_image(self.visible_images_path[self.current_image_index])

    @Slot()
    def prev_image(self):
        if self.current_image_index-1 >= 0:
            self.current_image_index -= 1
            self.show_image(self.visible_images_path[self.current_image_index])

    @Slot()
    def sort_thumbails(self):
        print("TODO : sort thumbnails by selection")

    @Slot()
    def select_image(self):
        images_path = [self.visible_images_path[self.current_image_index]]

        if self.move_raw_jpg_checkbox.isChecked():
            image_name = os.path.basename(images_path[0]).split('.')[0]
            images_path = [item for item in self.images_path if image_name in item]

        for image_path in images_path:
            if image_path not in self.images_selection["selected"]:
                if image_path in self.images_selection["rejected"]:
                    self.images_selection["rejected"].remove(image_path)
                self.images_selection["selected"].append(image_path)

        self.toggle_selection(images_path[0])

    @Slot()
    def reject_image(self):
        images_path = [self.visible_images_path[self.current_image_index]]

        if self.move_raw_jpg_checkbox.isChecked():
            image_name = os.path.basename(images_path[0]).split('.')[0]
            images_path = [item for item in self.images_path if image_name in item]

        for image_path in images_path:
            if image_path not in self.images_selection["rejected"]:
                if image_path in self.images_selection["selected"]:
                    self.images_selection["selected"].remove(image_path)
                self.images_selection["rejected"].append(image_path)
        self.toggle_selection(images_path[0])

    def toggle_selection(self, image_path):
        if image_path in self.images_selection["selected"]:
            self.select_button.setStyleSheet("background-color: #5DF6A4;")
            self.reject_button.setStyleSheet("")
        elif image_path in self.images_selection["rejected"]:
            self.reject_button.setStyleSheet("background-color: #F66C5D;")
            self.select_button.setStyleSheet("")
        else:
            self.select_button.setStyleSheet("")
            self.reject_button.setStyleSheet("")

    def update_thumbnails_color(self):
        thumbnail = None
        for i in range(self.grid_layout.count()):
            thumbnail = self.grid_layout.itemAt(i).widget()
            thumbnail_image_path = thumbnail.objectName()

            if thumbnail_image_path in self.images_selection["selected"]:
                thumbnail.setStyleSheet("background-color: #5DF6A4;")

            elif thumbnail_image_path in self.images_selection["rejected"]:
                thumbnail.setStyleSheet("background-color: #F66C5D;")

            else:
                thumbnail.setStyleSheet("")

    @Slot()
    def rotate_image(self):
        pixmap = self.photo_label.pixmap()

        if pixmap is not None:
            transform = QTransform()
            
            transform.rotate(90)
            rotated_pixmap = pixmap.transformed(transform, Qt.SmoothTransformation)
            scaled_pixmap = rotated_pixmap.scaled(self.photo_label.width(), self.photo_label.height(), Qt.KeepAspectRatio, Qt.SmoothTransformation)

            # Mettre à jour l'image dans le QLabel
            self.photo_label.setPixmap(scaled_pixmap)

    # TODO afficher une autre fenêtre
    # @Slot()
    # def validate_selection(self):
    #     if self.validate_window is None:
    #         self.validate_window = ValidateWindow(self.images_selection)
    #         self.validate_window.show()

    @Slot()
    def validate_selection(self):
        # Create directories for selected and rejected files
        selected_directory = Path(self.folder_path, "selected")
        selected_directory.mkdir(parents=True, exist_ok=True)

        rejected_directory = Path(self.folder_path, "rejected")  # Separate directory for rejected files
        rejected_directory.mkdir(parents=True, exist_ok=True)

        # Move selected files
        self.move_files(self.images_selection["selected"], selected_directory)
        
        # Move rejected files
        self.move_files(self.images_selection["rejected"], rejected_directory)

    def move_files(self, image_paths, destination_directory):
        for image_path in image_paths:
            source = Path(image_path)
            
            if source.exists():
                destination = destination_directory / source.name  # Using the `/` operator to create the path
                try:
                    shutil.move(str(source), str(destination))  # Move the file
                    print(f"Moved: {source} to {destination}")  # Log the move
                except Exception as e:
                    print(f"Error moving {source} to {destination}: {e}")
            else:
                print(f"Source file does not exist: {source}")

    @Slot()
    def open_folder_dialog(self):
        # TODO enlever le chemin vers ./data
        folder_path = QFileDialog.getExistingDirectory(self, "Open Folder", "/home/thomas/Workspace/picpicker/data/save")
        if folder_path:
            # Emettre le signal avec le chemin du dossier sélectionné
            self.folder_path = folder_path
            self.images_path = [file for file in glob.glob(folder_path + "/*") if os.path.isfile(file)]
            self.visible_images_path = sorted(self.images_path)
            self.stacked_widget.setCurrentIndex(1)
            self.display_thumbnails()