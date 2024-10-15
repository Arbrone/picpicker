import sys
from PySide6.QtWidgets import QApplication
from picpicker import PicPicker

if __name__ == "__main__":
    app = QApplication(sys.argv)
    startWidget = PicPicker()
    startWidget.show()

    with open("style.qss", "r") as f:
        _style = f.read()
        app.setStyleSheet(_style)
    
    sys.exit(app.exec())