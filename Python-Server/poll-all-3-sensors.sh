#!/bin/bash

SCRIPT_NAME=$(realpath "$0")
SCRIPT_PATH=$(dirname "${SCRIPT_NAME}")
LOGFILE="./sensor_polling_$(date "+%d.%m.%Y_%H:%M").log"

cd "${SCRIPT_PATH}"

SENSORS="alpha beta gamma"
PERIOD="7d"



creating_new_output_files() {
  for sensor in ${SENSORS};
  do
    echo "Polling now sensor ${sensor} with period ${PERIOD}." >> "${LOGFILE}"
    ./main.py --sensor ${sensor} --time-period ${PERIOD} >> "${LOGFILE}" 2>&1
    # sleep 3
  done
}

deleting_obsolete_files() {
  mkdir -p ${SCRIPT_PATH}/backup
  for sensor in ${SENSORS};
  do
    days=$(find . -name "${sensor}*.csv" | cut -d "-" -f 8-10 | cut -d "_" -f2- | sed  's/.csv//g' | sort | uniq)
    for day in ${days};
    do
      latest_csv_file=$(ls ./${sensor}*.csv | grep "${day}" | sort | tail -n1)
      mv "${latest_csv_file}" ${SCRIPT_PATH}/backup/
    done
    latest_json_file=$(ls ./${sensor}*.json | sort | tail -n1)
    mv "${latest_json_file}" ${SCRIPT_PATH}/backup/
  done
  rm *.csv 2>/dev/zero
  rm *.json 2>/dev/zero
  mv ${SCRIPT_PATH}/backup/* .
  rmdir "${SCRIPT_PATH}/backup"
}

creating_new_output_files
deleting_obsolete_files
