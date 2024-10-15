import sys
from PySide6.QtWidgets import QApplication
from picpicker import PicPicker
from PySide6.QtGui import QIcon

if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setWindowIcon(QIcon("logo.png"))
    startWidget = PicPicker()
    startWidget.show()

    with open("style.qss", "r") as f:
        _style = f.read()
        app.setStyleSheet(_style)
    
    sys.exit(app.exec())