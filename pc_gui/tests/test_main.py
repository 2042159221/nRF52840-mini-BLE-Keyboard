from pc_gui.main import missing_dependency_message


def test_missing_dependency_message_includes_install_command():
    message = missing_dependency_message("PySide6", "python.exe")

    assert "PySide6" in message
    assert "python.exe -m pip install -r pc_gui/requirements.txt" in message
