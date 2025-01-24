import os
import sys
import subprocess

# Comment out once paths are set
print("YOU NEED TO EDIT THIS THIS SCRIPT", file=sys.stderr)


# Set up environment variables
os.environ["PATH"] += ":<PATH TO IoTWeather/Code/tools>"  # Path to include the correct directory for arduino-cli

# Paths and FQBN details
cli_path = "<PATH TO IoTWeather/Code/tools/arduino-cli>"  # Path to arduino-cli binary
project_path = "<PATH TO  IoTWeather/Code/IoTWeather>"
fqbn = "esp32:esp32:XIAO_ESP32C3" # ("esp32:esp32:XIAO_ESP32C3", "esp32:esp32:adafruit_feather_esp32s2", "esp32:esp32:adafruit_qtpy_esp32c3")
ota_port = "/dev/ttyACM0"


# Compile Command
compile_cmd = [cli_path, "--log-level", "debug", "--verbose", "compile", "--fqbn", fqbn, project_path]

# Upload Ccmmand
upload_cmd = [cli_path, "upload", "--log-level", "debug", "--verbose", "--port", ota_port, "--fqbn", fqbn, "--verify"]

# Running commands sequentially
try:
    # Change directory to project directory
    os.chdir(os.path.expanduser(project_path))
    
    # Run Compile Command
    print("Running Compile Command...")

    # Using subprocess to stream output
    compile_process = subprocess.Popen(compile_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    for line in compile_process.stdout:
        print(line.decode().strip())  # Print each line of output as it happens
    compile_process.wait()  # Ensure the process completes

    # Check if there was an error in the compile process
    if compile_process.returncode != 0:
        raise subprocess.CalledProcessError(compile_process.returncode, compile_cmd)

    # Run Upload Command
    print("Running Upload Command...")
    upload_process = subprocess.Popen(upload_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    for line in upload_process.stdout:
        print(line.decode().strip())  # Print each line of output as it happens
    upload_process.wait()

    # Check if there was an error in the upload process
    if upload_process.returncode != 0:
        raise subprocess.CalledProcessError(upload_process.returncode, upload_cmd)

except subprocess.CalledProcessError as e:
    print(f"Error during process: {e}")
