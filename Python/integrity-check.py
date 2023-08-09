#! /usr/bin/env python3

import argparse
import glob
import hashlib
import pathlib
import re
import zlib

from typing import NamedTuple
from types import SimpleNamespace


# assumptions:
# - same modification time in history file and current state => no changes
# - same modification time in history file and current state but different checksum => file corruption

# TODO:
# - add option to sign and verify history file
# - add option to store extra data required to fix corrupted data


class Constants:
    IntegrityHistoryFileName = '.integrityhistory.txt'


class FileInfo(NamedTuple):
    sha256: str = ''
    modification_time: int = 0


FileInfoDict = dict[str, FileInfo]


class HistoryFile(SimpleNamespace):
    files: FileInfoDict = {}
    files_corrupted_entries: FileInfoDict = {}
    update_time: int = 0
    version: int = 1


# History file structure:
# const header: integrity history version 1
# const header: str(sha256 of all entries bellow)
# entry: str(sha256) int(timestamp_ns) str(crc32 of str(sha256)+str(timestamp_ns)+str(relative_path)) str(relative_path)
#
# Notes:
# - history file keeps relative path just to make easier sharing same history file between different copies


def history_file_read(path: pathlib.Path) -> HistoryFile:
    with open(path, 'rt', encoding='utf-8') as file:
        files: FileInfoDict = {}
        files_corrupted_entries: FileInfoDict = {}
        version_match = re.fullmatch('integrity history version ([0-9]+)', file.readline().rstrip('\n'))
        if not version_match:
            raise IOError('File is not a valid history file or file is corrupted: ' + str(file))
        version = int(version_match[1])
        if version != 1:
            raise IOError('Not supported history file version or file is corrupted: ' + str(file))
        header_lines_count = 1
        for index, text in enumerate(file):
            line_number = index + header_lines_count + 1
            text = text.rstrip('\n')
            if not text:
                continue
            file_info_match = re.fullmatch(r'(\S+)\s+(\S+)\s+(\S+)\s+(.+)', text)
            if not file_info_match:
                print(f'WARN: Skipping corrupted entry. Invalid integrity history entry at line {line_number}: {str(path)}')
                print(f'      entry: "{text}"')
                files_corrupted_entries[text] = FileInfo()
                continue
            sha256 = file_info_match[1]
            modification_time = file_info_match[2]
            crc32 = file_info_match[3].lower()
            file_path = file_info_match[4]
            current_crc32 = hex(zlib.crc32((sha256 + modification_time + file_path).encode(encoding='utf-8'))).lower()[2:]
            file_info = FileInfo(sha256, int(modification_time) if modification_time.isnumeric() else 0)
            if crc32 != current_crc32 or not modification_time.isnumeric():
                print(f'WARN: Skipping corrupted entry. Invalid CRC for entry at line {line_number}: {str(path)}:')
                print(f'      sha256="{sha256}" mtime="{str(modification_time)}" crc32="{crc32}" path="{file_path}"')
                files_corrupted_entries[file_path] = file_info
                continue
            if file_path in files:
                print(f'WARN: Skipping entry. Duplicated file entry in integrity history: {str(path)}: {file_path}')
                files_corrupted_entries[file_path] = file_info
                continue
            files[file_path] = file_info
        return HistoryFile(files=files, files_corrupted_entries=files_corrupted_entries, update_time=path.stat().st_mtime_ns, version=version)


def history_file_write(path: pathlib.Path, history: HistoryFile):
    with open(path, 'wt', encoding='utf-8') as file:
        file.write('integrity history version 1\n')
        for file_path, value in history.files.items():
            crc32 = hex(zlib.crc32((value.sha256 + str(value.modification_time) + file_path).encode(encoding='utf-8'))).lower()[2:]
            file.write(f"{value.sha256} {value.modification_time} {crc32} {file_path}\n")
    history.update_time = path.stat().st_mtime_ns


def history_file_move_olds(path: pathlib.Path):
    # TODO:
    # - keep history file versions (for 1 year, 1 month, 1 week, 1 day, 1 hour) - that means:
    #   - one file per year for history older than one year,
    #   - one per month for older than one month,
    #   - one per week for older than one week,
    #   - one per day for older than one day,
    #   - one per hour for older than one hour,
    #   - one per quarter of an hour
    #   - single file for latest updated after any call
    pass


def progress_bar(current: int, total: int, width: int = 20):
    fill = int(width * current / total)
    print("[{}{}] {}/{}".format("#" * fill, " " * (width - fill), current, total), end='\r', flush=True)


def progress_bar_end():
    print('')


def get_current_state(directory: pathlib.Path) -> HistoryFile:
    files = {}
    progress_bar(0, 1)
    all_paths = glob.glob(str(directory.joinpath('**', '*')), recursive=True)
    all_paths_count = len(all_paths)
    for index, path in enumerate(all_paths):
        progress_bar(index + 1, all_paths_count)
        path = pathlib.Path(path)
        if not path.is_file():
            continue
        rel_path = path.relative_to(directory)
        with open(path, "rb") as f:
            hash = hashlib.file_digest(f, "sha256")
        files[str(rel_path)] = FileInfo(hash.hexdigest(), path.stat().st_mtime_ns)
    progress_bar_end()
    return HistoryFile(files=files)


def get_old_state(history_file: pathlib.Path) -> HistoryFile:
    if not history_file.exists():
        return HistoryFile()
    return history_file_read(history_file)


def integrity_check(directory: pathlib.Path, history_file: pathlib.Path):
    print('Reading history...')
    old_state = get_old_state(history_file)
    print('Checking current state...')
    current_state = get_current_state(directory)
    print('Validating...')

    changed_files = []
    corrupted_files = []
    deleted_files = []
    new_files = []

    for file_path, file_old_state in old_state.files.items():
        file_current_state = current_state.files.get(file_path)
        if not file_current_state:
            deleted_files.append(file_path)

    for file_path, file_current_state in current_state.files.items():
        file_old_state = old_state.files.get(file_path)

        if file_old_state:
            if file_current_state.modification_time != file_old_state.modification_time:
                changed_files.append(file_path)
                continue
            if file_current_state.sha256 == file_old_state.sha256:
                continue
        elif not file_path in old_state.files_corrupted_entries:
            new_files.append(file_path)
            continue
        corrupted_files.append(file_path)

    def print_list(header: str, data: list[str]):
        print(header)
        for entry in data:
            print('-', entry)

    if len(new_files) + len(deleted_files) + len(changed_files) + len(corrupted_files) == 0:
        print('Nothing changed')
        return 0

    if len(corrupted_files) > 0:
        print_list('Corrupted files:', corrupted_files)
        return -1

    if len(new_files) > 0:
        print_list('New files:', new_files)
    if len(deleted_files) > 0:
        print_list('Deleted files:', deleted_files)
    if len(changed_files) > 0:
        print_list('Changed files:', changed_files)

    if len(old_state.files_corrupted_entries) > 0:
        print('ERROR: Can\'t update history file - old history file contains corrupted entries, '
              'please check entries and files manually, restore from backup '
              'or delete invalid entry from history and try again.')
        return -1

    history_file_move_olds(history_file)
    history_file_write(history_file, current_state)
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory")
    parser.add_argument("--history-file", help=f"Custom location of history file, for default history is stored "
                                               f"in indicated directory in file {Constants.IntegrityHistoryFileName}")
    args = parser.parse_args()

    target_directory = pathlib.Path(args.directory)
    if not (target_directory.exists() and target_directory.is_dir()):
        parser.exit(-1, f'error: passed argument is not a valid directory path: {args.directory}')

    target_history_file = pathlib.Path(args.history_file) if args.history_file else None
    if not target_history_file:
        target_history_file = target_directory.joinpath(Constants.IntegrityHistoryFileName)

    if target_history_file.exists() and not target_history_file.is_file():
        parser.exit(-1, f'error: passed history file path is not valid: {str(target_history_file)}')

    return integrity_check(target_directory, target_history_file)


if '__main__' == __name__:
    exit(main())
