import sys
import glob
import os
from thumbnail import ThumbailsWidget
from imageViewer import ImageViewerWidget
from folderOpener import FolderOpenerWidget
from validateSelection import ValidateSelectionWidget
from PySide6.QtCore import Qt, Slot, QTimer
from PySide6.QtWidgets import (QApplication,
                               QMainWindow,
                               QHBoxLayout,
                               QVBoxLayout,
                               QPushButton,
                               QGroupBox,
                               QCheckBox, 
                               QWidget,
                               QStackedWidget)
from PySide6.QtGui import (QScreen, 
                           QShortcut)

class PicPicker(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PicPicker")
        self.resize(1600, 900)

        self.folder_path = None
        self.images_path = None
        self.visible_images_path = None
        self.images_selection = {"selected":[],
                                 "rejected":[]}

        self.main_widget = QWidget()

        self.folder_widget = FolderOpenerWidget(self.folder_path_callback)
        self.thumbnails_widget = ThumbailsWidget(self.thumbnails_loaded_callback, self.image_clicked_callback)
        self.image_widget = ImageViewerWidget()
        self.stacked_widget = QStackedWidget()
        self.stacked_widget.addWidget(self.folder_widget)
        self.stacked_widget.addWidget(self.thumbnails_widget)
        self.stacked_widget.addWidget(self.image_widget)

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
        #self.sort_button.clicked.connect(self.sort_thumbails)
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
        # self.left_panel.addWidget(self.sort_button)
        self.left_panel.addStretch()
        self.left_panel.addWidget(self.validate_button)  # Ajouter le bouton validate en bas

        self.main_layout = QHBoxLayout()
        self.main_layout.addLayout(self.left_panel, 2)
        self.main_layout.addWidget(self.stacked_widget, 10) 
        self.main_widget.setLayout(self.main_layout)

        self.setCentralWidget(self.main_widget)
        self.set_left_panel_state(False)

        # SHORTCUTS
        self.next_image_shortcut = QShortcut(Qt.Key.Key_Right,self.image_widget.photo_label)
        self.next_image_shortcut.activated.connect(self.next_image)

        self.prev_image_shortcut = QShortcut(Qt.Key.Key_Left,self.image_widget.photo_label)
        self.prev_image_shortcut.activated.connect(self.prev_image)

        self.hide_shortcut = QShortcut(Qt.Key.Key_Escape, self.image_widget.photo_label)
        self.hide_shortcut.activated.connect(self.hide_photo)

        self.select_shortcut = QShortcut(Qt.Key.Key_Up, self.image_widget.photo_label)
        self.select_shortcut.activated.connect(self.select_image)
        self.reject_shortcut = QShortcut(Qt.Key.Key_Down, self.image_widget.photo_label)
        self.reject_shortcut.activated.connect(self.reject_image)

    def set_left_panel_state(self, state:bool):
        for i in range(self.left_panel.count()):
            widget = self.left_panel.itemAt(i).widget()
            if widget is not None:
                widget.setEnabled(state)

        for i in range(self.selection_image_layout.count()):
            widget = self.selection_image_layout.itemAt(i).widget()
            if widget is not None:
                widget.setEnabled(state)


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
        
        self.thumbnails_widget.display_thumbnails(self.visible_images_path)
        self.thumbnails_widget.update_thumbnails_color(self.images_selection)

    Slot(str)
    def image_clicked_callback(self, image_path):
        width, height = self.stacked_widget.width(), self.stacked_widget.height()
        self.current_image_index = self.visible_images_path.index(image_path)
        self.image_widget.show_image(image_path, width, height)
        self.stacked_widget.setCurrentIndex(self.stacked_widget.currentIndex() + 1)

    @Slot(str)
    def folder_path_callback(self, folder_path):
        self.folder_path = folder_path
        self.images_path = [file for file in glob.glob(folder_path + "/*") if os.path.isfile(file)]
        self.visible_images_path = sorted(self.images_path)

        self.stacked_widget.setCurrentIndex(self.stacked_widget.currentIndex()+1)
        self.thumbnails_widget.display_thumbnails(self.visible_images_path)

    @Slot()
    def thumbnails_loaded_callback(self):
        self.thumbnails_widget.update_thumbnails_color(self.images_selection)
        self.set_left_panel_state(True)

    @Slot()
    def hide_photo(self):
        self.stacked_widget.setCurrentWidget(self.thumbnails_widget)
        self.select_button.setStyleSheet("")
        self.reject_button.setStyleSheet("")
        self.thumbnails_widget.update_thumbnails_color(self.images_selection)

    @Slot()
    def next_image(self):
        if self.current_image_index+1 < len(self.visible_images_path):
            self.current_image_index += 1
            width, height = self.stacked_widget.width(), self.stacked_widget.height()
            self.image_widget.show_image(self.visible_images_path[self.current_image_index], width, height)
            self.toggle_selection(self.visible_images_path[self.current_image_index])

    @Slot()
    def prev_image(self):
        if self.current_image_index-1 >= 0:
            self.current_image_index -= 1
            width, height = self.stacked_widget.width(), self.stacked_widget.height()
            self.image_widget.show_image(self.visible_images_path[self.current_image_index], width, height)
            self.toggle_selection(self.visible_images_path[self.current_image_index])

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
        QTimer.singleShot(25, self.next_image)

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
        QTimer.singleShot(25, self.next_image)

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

    @Slot()
    def validate_selection(self):
        # TODO case if images_selection is empty
        self.validate = ValidateSelectionWidget(self.folder_path, self.images_selection)
        self.validate.show()