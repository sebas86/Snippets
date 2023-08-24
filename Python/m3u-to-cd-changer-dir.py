#! /usr/bin/env python3

import argparse
import pathlib
import shutil


class Counter:
    def __init__(self, initial_value: int):
        self.next_value = initial_value

    def get_next(self) -> int:
        result = self.next_value
        self.next_value += 1
        return result


def log(pretend: bool, verbose: bool, *args):
    if pretend or verbose:
        print(*args)


def read_playlist(playlist: pathlib.Path) -> list[pathlib.Path]:
    result = []
    with open(playlist, 'rt', encoding='utf-8') as file:
        for line in file:
            line = line.rstrip('\n')
            if not line or '#' == line[0]:
                continue
            entry = pathlib.Path(line)
            if entry.is_absolute():
                result.append(entry)
            else:
                result.append(playlist.parent.joinpath(entry).absolute())
    return result


def process_playlist(playlist: pathlib.Path, target_dir: pathlib.Path, counter: Counter = Counter(1), pretend: bool = False, verbose: bool = False):
    tracks = read_playlist(playlist)
    log(pretend, verbose, str(playlist), ':')
    for source_path in tracks:
        target_path = target_dir.joinpath(f"{counter.get_next():04}" + str(source_path.suffix))
        log(pretend, verbose, '-', str(source_path), '->', str(target_path))
        if not pretend:
            shutil.copy(str(source_path), str(target_path))
    return 0


def main():
    parser = argparse.ArgumentParser(
        description='This script copy files listed on playlist to target directory and renames file to number')
    parser.add_argument('output_dir', type=pathlib.Path)
    parser.add_argument('playlists', metavar='playlist.m3u', nargs='+', type=pathlib.Path)
    parser.add_argument('--counter', metavar='initial value', type=int, default=1)
    parser.add_argument('--pretend', action='store_true')
    parser.add_argument('--verbose', action='store_true')
    args = parser.parse_args()

    target_directory = args.output_dir
    playlists = args.playlists
    pretend = args.pretend
    verbose = args.verbose
    counter = Counter(args.counter)

    for playlist in playlists:
        process_playlist(playlist, target_directory, counter, pretend, verbose)

    return 0


if '__main__' == __name__:
    exit(main())
