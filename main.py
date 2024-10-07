import sys
from PySide6.QtWidgets import QApplication
from picpicker import PicPicker

if __name__ == "__main__":
    app = QApplication(sys.argv)
    startWidget = PicPicker()#("/home/thomas/Workspace/picpicker/data/save")
    startWidget.show()
    sys.exit(app.exec())