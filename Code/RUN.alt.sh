#!/usr/bin/env bash

# Navigate to the project directory
cd IoTWeather

# Create a virtual environment if it doesn't exist
if [ ! -d "venv" ]; then
	python3 -m venv venv
fi

# Activate the virtual environment
source venv/bin/activate

# Example of installing pyserial (if needed for esptool issues or serial support)
pip install pyserial

# Run the Python script from within the virtual environment
python3 ../build_and_upload.alt.py

# Deactivate the virtual environment
deactivate

# Check for flags (-k to keep, -d to delete)
KEEP_VENV=false
DELETE_VENV=false
while getopts "kd" opt; do
	case ${opt} in
		k )
			KEEP_VENV=true
			;;
		d )
			DELETE_VENV=true
			;;
		\? )
			echo "Invalid option: -$OPTARG" >&2
			exit 1
			;;
	esac
done

# Handle automatic keep or delete based on flags
if $KEEP_VENV; then
	echo "Keeping the virtual environment folder (venv)."
elif $DELETE_VENV; then
	echo "Deleting the virtual environment folder..."
	\rm -rf ./venv
else
	# Ask for confirmation to delete the venv folder if no flags provided
	read -p "Do you want to delete the virtual environment folder (venv)? [Y/N] (default: N): " confirm

	# Default to 'N' if no input is provided
	confirm=${confirm:-N}

	if [[ "$confirm" =~ ^[Yy]$ ]]; then
		echo "Deleting the virtual environment folder..."
		\rm -rf ./venv
	else
		echo "Keeping the virtual environment folder."
	fi
fi

# Return to the previous directory silently
cd - > /dev/null
