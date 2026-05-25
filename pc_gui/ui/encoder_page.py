from __future__ import annotations

from PySide6.QtWidgets import QComboBox, QFormLayout, QHBoxLayout, QPushButton, QVBoxLayout, QWidget

ACTION_LABELS = [
    "None",
    "Volume up",
    "Volume down",
    "Mute",
    "Play / pause",
    "Next track",
    "Previous track",
    "RGB mode next",
]


class EncoderPage(QWidget):
    def __init__(self, window) -> None:
        super().__init__()
        self.window = window
        self.cw = self._combo()
        self.ccw = self._combo()
        self.press = self._combo()
        load_button = QPushButton("Load")
        apply_button = QPushButton("Apply")
        load_button.clicked.connect(self.load_config)
        apply_button.clicked.connect(self.apply_config)

        buttons = QHBoxLayout()
        buttons.addWidget(load_button)
        buttons.addWidget(apply_button)
        buttons.addStretch(1)

        form = QFormLayout()
        form.addRow("Clockwise", self.cw)
        form.addRow("Counter-clockwise", self.ccw)
        form.addRow("Press", self.press)

        layout = QVBoxLayout(self)
        layout.addLayout(buttons)
        layout.addLayout(form)
        layout.addStretch(1)

    @staticmethod
    def _combo() -> QComboBox:
        combo = QComboBox()
        combo.addItems(ACTION_LABELS)
        return combo

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
        self.cw.setCurrentIndex(config.encoder_cw_action)
        self.ccw.setCurrentIndex(config.encoder_ccw_action)
        self.press.setCurrentIndex(config.encoder_press_action)

    def apply_config(self) -> None:
        config = self.window.current_config_copy()
        if config is None:
            return
        config.encoder_cw_action = self.cw.currentIndex()
        config.encoder_ccw_action = self.ccw.currentIndex()
        config.encoder_press_action = self.press.currentIndex()
        self.window.apply_config(config)
