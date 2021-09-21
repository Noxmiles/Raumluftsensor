#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Bachelorarbeit Alexander Ochs, Innenraumluft
import sys
import requests
import json
import re
import argparse
from datetime import datetime
from http import HTTPStatus

STARTUP_TIMESTAMP = datetime.now().strftime('%Y-%m-%d-%H-%M-%S')

# Constants
HTTPS_ACCESS_TOKEN = 'ttn-account-v2.xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx-xxxxxxxx'
SENSOR_MODEL = 'mkr1310'
AVAILABLE_SENSORS = ['alpha', 'beta', 'gamma']
MAX_TIME_PERIOD = 7 * 24 * 60  # Minuten
# Example full URL: https://xxxx-application-xxxxxxxxxxxx.data.thethingsnetwork.org/api/v2/query/mkr1310-alpha?last=7d
BASE_URL = 'https://xxxx-application-xxxxxxxxxxxx.data.thethingsnetwork.org/api/v2/query'
CSV_SEPARATOR = ';'


def convert_to_minutes(period_string):
    unit = period_string[-1]
    period_numeric = int(period_string[0:len(period_string) - 1])  # Remove last char since it's the unit
    if unit == 'd':
        return period_numeric * 24 * 60
    elif unit == 'h':
        return period_numeric * 60
    else:
        return period_numeric


def check_time_period(time_period):
    if not re.match(r'[1-9][0-9]{0,3}[dhm]', time_period):
        print('ERROR: Period has to be given in days, hours or minutes, exit now!')
        sys.exit(2)
    if not (1 <= convert_to_minutes(time_period) <= MAX_TIME_PERIOD):
        print(f'ERROR: Give a positive value for the time period smaller than the maximum ({MAX_TIME_PERIOD})')
    # Everything is fine
    return time_period


# Funny CLI arguments' like a pro
def parse_arguments(cli_arguments):
    parser = argparse.ArgumentParser('Sensor Data Downloader and Converter',
                                     description='''
                                     Downloads IOT sensor data from "The Things Network" as json files
                                     and convert them in CSV. 
                                     ''')

    parser.add_argument('-s', '--sensor', required=True, type=str, dest='sensor', choices=AVAILABLE_SENSORS,
                        help='The name of the sensor, <{}>'.format('|'.join(AVAILABLE_SENSORS)))
    parser.add_argument('-t', '--time-period', required=True, type=check_time_period, dest='period',
                        help='Since when you want to retrieve the data? (Max: 7d)')
    parser.add_argument('-i', '--input-file', default='', required=False, type=str, dest='input_file',
                        help='Absolute path to a local input json file to convert to CSV. '
                             'If one is given, the download from TheThingsNetwork is skipped.')
    parser.add_argument('-j', '--create-day-jsons', required=False, action='store_true', dest='create_day_jsons',
                        help='Whether to create a json for every single polled day '
                             '(by default, only create CSVs per day)')
    return parser.parse_args(cli_arguments)


# Aufbau der URL
def determine_full_url(sensor_name, last_period):
    return '{base}/{sensor_model}-{sensor_name}?last={last_period}'.format(base=BASE_URL,
                                                                           sensor_model=SENSOR_MODEL,
                                                                           sensor_name=sensor_name,
                                                                           last_period=last_period)


def determine_output_filename(sensor, period, file_extension='json', day=None):
    # format: alpha-2020-07-15-17-49-7d.json
    global STARTUP_TIMESTAMP
    day_suffix = f'_{day}' if day is not None else ''
    return f'{sensor}-{STARTUP_TIMESTAMP}-{period}{day_suffix}.{file_extension}'


def create_json_file(output_filename, data, dump_data_directly=False):
    print('Now writing to JSON file: {}'.format(output_filename))
    with open(output_filename, 'w') as output_file:
        if dump_data_directly:
            json.dump(data, output_file)
        else:
            for dataset in data:
                json.dump(dataset, output_file)


# Order of entries ! ! ! 
def get_id_of_field(field):
    if re.match(r'.*_[0-9]+', field):
        return int(field.split('_')[-1])
    else:
        # special handling of fields
        field_id_mapping = {
            'device_id': 99,
            'raw': 98,
            'time': 0
        }
        return field_id_mapping[field]


# entries that will be deleted
def blacklist_headers(headers):
    blacklist = ['device_id', 'raw']
    for blacklisted_header in blacklist:
        if blacklisted_header in headers:
            headers.remove(blacklisted_header)
    return headers


def sort_headers(headers):
    return sorted(headers, key=get_id_of_field)


# convert names from original CayenneLPP format to correct
def map_headers(headers):
    header_mapping = {
        'barometric_pressure_1': 'Luftdruck_BME',
        'luminosity_2': 'WiderstandGaskOhm_BME',
        'digital_in_3': 'iAQaccuracy_BME',
        'temperature_4': 'Temperatur_BME',
        'relative_humidity_5': 'Luftfeuchte_BME',
        'luminosity_6': 'sIAQ_BME',
        'luminosity_7': 'eCO2_BME',
        'analog_in_8': 'eTVOC_BME',
        'luminosity_9': 'eCO2_IAQ',
        'luminosity_10': 'eTVOC_IAQ',
        'luminosity_11': 'WiderstandkOhm_IAQ',
        'digital_in_12': 'Status_IAQ',
        'luminosity_13': 'Status_BME',
        'luminosity_14': 'Status_BSEC'
    }
    mapped_headers = []
    for header in headers:
        mapped_headers.append(header_mapping.get(header, header))
    return mapped_headers


def do_time_modification(data):
    # 2020-08-23T09:55:02.047630666Z -> 09:55:02
    full_time = data.split('T')[1]
    hours_minutes_seconds = full_time.split('.')[0]
    return hours_minutes_seconds


def get_field_data(dataset, field):
    modified_data = str(dataset.get(field, ''))
    field_modifiers = {
        'time': do_time_modification
    }
    if field in field_modifiers:
        modified_data = field_modifiers[field](modified_data)
    return modified_data


def create_csv_file(output_filename, data):
    print('Now writing to CSV file: {}'.format(output_filename))
    blacklisted_headers = blacklist_headers(list(data[0].keys()))
    sorted_headers = sort_headers(blacklisted_headers)
    mapped_headers = map_headers(sorted_headers)
    header_line = CSV_SEPARATOR.join(mapped_headers) + '\n'
    with open(output_filename, 'w') as output_file:
        output_file.write(header_line)
        for dataset in data:
            data_line = CSV_SEPARATOR.join(
                get_field_data(dataset, header_field) for header_field in sorted_headers) + '\n'
            output_file.write(data_line)


# Download and save JSON
def save_files(http_response_json, sensor, period, create_day_jsons):
    # Save original downloaded JSON
    json_full_filename = determine_output_filename(sensor, period)
    create_json_file(json_full_filename, http_response_json, dump_data_directly=True)

    # Save 1 JSON per day
    data_per_day = {}
    for json_object in http_response_json:
        whole_timestamp = json_object['time']
        date_only = whole_timestamp[0:whole_timestamp.index('T')]
        if date_only in data_per_day.keys():
            data_per_day[date_only].append(json_object)
        else:
            data_per_day[date_only] = [json_object]
    for day in data_per_day.keys():
        day_measurements = data_per_day[day]
        csv_day_filename = determine_output_filename(sensor, period, file_extension='csv', day=day)
        if create_day_jsons:
            json_day_filename = determine_output_filename(sensor, period, day=day)
            create_json_file(json_day_filename, day_measurements)
        create_csv_file(csv_day_filename, day_measurements)


def get_http_resource_as_json(url):
    http_headers = {
        'Accept': 'application/json',
        'Authorization': 'key {token}'.format(token=HTTPS_ACCESS_TOKEN)
    }
    print(f'Now trying to do a HTTP GET-Request to this URL: "{url}" with these headers: {http_headers}')
    response = requests.get(url, headers=http_headers)
    if response.status_code == HTTPStatus.OK:
        return response.json()
    else:
        print('HTTP Get-Request failed, got Status code {}'.format(response.status_code))
        sys.exit(1)


# + + + main starts here + + +
def main(args):
    parsed_args = parse_arguments(args[1:])
    if len(parsed_args.input_file) > 0:
        # Input file was given, so use local input file instead of downloading a json from TTN
        print(f'Converting local file "{parsed_args.input_file}", skipping download!')
        with open(parsed_args.input_file, 'r') as json_input:
            json_to_convert = json.load(json_input)
    else:
        url_to_download = determine_full_url(parsed_args.sensor, parsed_args.period)
        json_to_convert = get_http_resource_as_json(url_to_download)
    save_files(json_to_convert, parsed_args.sensor, parsed_args.period, parsed_args.create_day_jsons)


if __name__ == "__main__":
    main(sys.argv)
