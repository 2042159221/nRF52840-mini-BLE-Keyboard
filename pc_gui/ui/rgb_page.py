from __future__ import annotations

from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QFormLayout,
    QHBoxLayout,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)


class RgbPage(QWidget):
    def __init__(self, window) -> None:
        super().__init__()
        self.window = window
        self.enabled = QCheckBox()
        self.mode = QComboBox()
        self.mode.addItems(["Off", "Static", "Key reactive"])
        self.brightness = self._spin(0, 100)
        self.red = self._spin(0, 255)
        self.green = self._spin(0, 255)
        self.blue = self._spin(0, 255)
        self.numlock = QComboBox()
        self.numlock.addItems(["Follow host", "Always white", "Off when low power"])

        load_button = QPushButton("Load")
        apply_button = QPushButton("Apply")
        load_button.clicked.connect(self.load_config)
        apply_button.clicked.connect(self.apply_config)

        buttons = QHBoxLayout()
        buttons.addWidget(load_button)
        buttons.addWidget(apply_button)
        buttons.addStretch(1)

        form = QFormLayout()
        form.addRow("Enabled", self.enabled)
        form.addRow("Mode", self.mode)
        form.addRow("Brightness", self.brightness)
        form.addRow("Red", self.red)
        form.addRow("Green", self.green)
        form.addRow("Blue", self.blue)
        form.addRow("Num Lock", self.numlock)

        layout = QVBoxLayout(self)
        layout.addLayout(buttons)
        layout.addLayout(form)
        layout.addStretch(1)

    @staticmethod
    def _spin(minimum: int, maximum: int) -> QSpinBox:
        box = QSpinBox()
        box.setRange(minimum, maximum)
        return box

    def load_config(self) -> None:
        config = self.window.current_config_copy()
        if config is None:
            self.window.run_device_task(
                "get_config",
                lambda client: client.get_config(),
                self._show_config,
            )
            return
        self._show_config(config)

    def _show_config(self, config) -> None:
        self.enabled.setChecked(config.rgb_enable)
        self.mode.setCurrentIndex(config.rgb_mode)
        self.brightness.setValue(config.rgb_brightness)
        self.red.setValue(config.rgb_red)
        self.green.setValue(config.rgb_green)
        self.blue.setValue(config.rgb_blue)
        self.numlock.setCurrentIndex(config.numlock_policy)

    def apply_config(self) -> None:
        config = self.window.current_config_copy()
        if config is None:
            return
        config.rgb_enable = self.enabled.isChecked()
        config.rgb_mode = self.mode.currentIndex()
        config.rgb_brightness = self.brightness.value()
        config.rgb_red = self.red.value()
        config.rgb_green = self.green.value()
        config.rgb_blue = self.blue.value()
        config.numlock_policy = self.numlock.currentIndex()
        self.window.apply_config(config)
