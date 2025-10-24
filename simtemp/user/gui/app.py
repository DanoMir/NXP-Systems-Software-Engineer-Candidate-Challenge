import os
import sys
import select
import struct
import time

from datetime import datetime, timezone
from PySide6.QtWidgets import QApplication, QWidget, QVBoxLayout, QLabel, QLineEdit, QPushButton
from PySide6.QtCore import QTimer, QObject, Signal, Slot, QTime, QDate
from PySide6.QtGui import QColor, QPalette

# Import functions
from your_utils_file import write_sysfs, read_and_print_sample, DEVICE_PATH, SAMPLE_SIZE, STRUCT_FORMAT, FLAG_THRESHOLD_CROSSED
TEMP_DIVISOR = 1000.0

#ASINCHRONOUS POLLING

class SensorWorker(QObject):
    """ Handles polling of I/O in a separated thread or uses a timer of Qt"""
    
    #Singals to comunicate to GUI
    new_data_signal = Signal(float, bool)
    alert_signal = Signal(bool)
    
    def __init__(self, fd, parent=None):
        super().__init__(parent)
        self.fd = fd
        self.poller = select.poll()
        self.poller.register(self.fd, select.POLLIN | select.POLLPRI)
        
        #Use QTimer to call to Polling in safe mode in the thread
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.check_for_events)
        self.timer.start(100) #Checking each 100ms
        
    @Slot()
    def check_for_events(self):
        """Calls to poll() and process the data/events"""
        try:
            #Wait quickly for events (timeout=0 so as to QTimer calls peridically)
            events = self.poller.poll(0)
            
            if events:
                for (event_fd, mask) in events:
                    if event_fd == self.fd:
                        
                        #1. Notifies the alert
                        if mask & select.POLLPRI:
                            self.alert_signal.emit(True)
                            
                        #2. Fast Empty Loop.
                        While(True):
                            try:
                                #Reading Logic and unpacking must be copied from read_and_print_sample
                                data = os.read(self.fd, SAMPLE_SIZE)
                                if len(data) != SAMPLE_SIZE: break
                                
                                #Unpacking and obtains temperature
                                timestamp_ns, temp_mC, flags = struct.unpack(STRUCT_FORMAT,data)
                                temp_C = temp_mC / TEMP_DIVISOR
                                
                                #Data Signal
                                self.new_data_signal.emit(temp_C, bool(flags & FLAG_THRESHOLD_CROSSED))
                            except BlockingIOError:
                                break
                            except Exception as e:
                                print("Reading worker Error: {e}")
                                break
        except Exception as e:
            print(f"Poll ERROR: {e}")
            self.timer.stop()
            
#GUI

class SensorDashBoard(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("NXP Virtual Sensor Dashboard")
        self.setFixedWidth(400)
        self.init_ui()
        self.fd = -1 #in file decriptor
        self.worker = None
        
    def init_ui(self):
        layout = QVBoxLayout()
        
        #Temperature Display
        self.temp_label = QLabel("Temperature: --.- °C")
        self.temp_label.setStyleSheet("font-size: 24pt; color: white; background-color: #333333; padding: 10px;")
        layout.addWidget(self.temp_label)
        
        #Visual Alert
        self.alert_status = QLabel("NORMAL STATE")
        self.alert_status.setAlignment(Qt.AlignCenter)
        self.set_alert_style(False)
        layout.addWidget(self.alert_status)
        
        # Control Panel (Control Path Interaction)
        control_group = QGroupBox("Control Path: Sysfs Configuration")
        control_layout = QHBoxLayout(control_group)
        
        ttk.Label(control_group, text="Sampling Rate (ms):").setStyleSheet("font-size: 10pt;")
        control_layout.addWidget(QLabel("Sampling Rate (ms): "))
        
        self.sampling_input = QLineEdit("100")
        control_layout.addWidget(self.sampling_input)
        
        self.apply_button = QPushButton("Apply to Sysfs")
        self.apply_button.clicked.connect(self.apply_sampling_rate)
        control_layout.addWidget(self.apply_button)
        
        layout.addWidget(control_group)
    
    def set_alert_style(self, is_alert):
        """ Applies visual styles based en the alert state. """
        if is_alert:
            style = "font-size: 16pt; color: black; background-color: #FF6347; padding: 10px; border-radius: 5px;"
            text = "!!! THRESHOLD CROSSED - ALERT !!!"
        else:
            style = "font-size: 16pt; color: white; background-color: #4CAF50; padding: 10px; border-radius: 5px;"
            text = "SYSTEM NORMAL"
        
        self.alert_status.setStyleSheet(style)
        self.alert_status.setText(text)

    # --- SLOTS (Conexiones del Worker) ---
    
    @Slot(float, bool)
    def update_temperature_display(self, temp_C, is_alert):
        """ Update the temperature and the color based in Worker dates. """
        self.temp_label.setText(f"Temperature: {temp_C:.2f} °C")
        
        # Cambiar el color de fondo para indicar una alerta inmediata (Flag en el sample)
        if is_alert:
            self.temp_label.setStyleSheet("font-size: 32pt; color: white; background-color: #FF0000; border-radius: 10px; padding: 20px;")
        else:
            self.temp_label.setStyleSheet("font-size: 32pt; color: white; background-color: #333333; border-radius: 10px; padding: 20px;")
            self.set_alert_style(False) # Restablecer el estado de alerta general
        
    @Slot(bool)
    def handle_pollpri_alert(self, status):
        """ Manages the POLLPRI notification (high priority event from kernel). """
        # Mantiene el estado de ALERTA general hasta que el sistema lo borre (Clear)
        self.set_alert_style(True)

    # --- Control Path Method (Sysfs Write) ---
    
    @Slot()
    def apply_sampling_rate(self):
        """ Writes the sampling configuration to Sysfs file. """
        new_value = self.sampling_input.text()
        print(f"Trying to write... '{new_value}' en {SYSFS_PATH}")
        try:
            with open(SYSFS_PATH, 'w') as f:
                f.write(new_value)
            print(" Sysfs Control writing  successful.")
        except FileNotFoundError:
            print(f"ERROR: Control Path {SYSFS_PATH} not found. (Simulation)")
        except Exception as e:
            print(f"ERROR to write in Sysfs: {e}")
                    
    def closeEvent(self, event):
        """ Close the File Descriptor to close the aplication. """
        if self.fd != -1:
            os.close(self.fd)
            print("File Descriptor closed to exit.")
        event.accept()
        
        
# =========================================================
# MAIN EXECUTION
# =========================================================
if __name__ == "__main__":
    app = QApplication(sys.argv)
    dashboard = SensorDashBoard()
    dashboard.show()
    sys.exit(app.exec())