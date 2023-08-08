#! /usr/bin/env python3

import argparse
import glob
import os
import pathlib
import re
import shutil


class DefaultArguments:
    KeepVersions = 3


def natural_sort_key(s, _nsre=re.compile('([0-9]+)')):
    return [int(text) if text.isdigit() else text.lower()
            for text in _nsre.split(s)]


def get_related_files(base_dir: pathlib.Path, version_postfix: str):
    pattern = str(base_dir.joinpath("*" + version_postfix + "*"))
    return glob.glob(pattern)


def log(pretend: bool, verbose: bool, *args):
    if pretend or verbose:
        print(*args)


def cleanup(base_dir: pathlib.Path, keep_versions: int, pretend: bool, verbose: bool):
    assert (keep_versions >= 0)
    versions = []
    for path in glob.glob(str(base_dir.joinpath("vmlinuz-*")), recursive=False):
        versions.append(re.match(r".*vmlinuz-(.*)", path)[1])
    versions.sort(key=natural_sort_key)

    if not pretend:
        removed_dir_path = base_dir.joinpath("removed")
        if not os.path.isdir(removed_dir_path):
            os.mkdir(removed_dir_path)

    versions_to_remove = versions[0: max(0, len(versions) - keep_versions)]
    versions_to_keep = list(set(versions) - set(versions_to_remove))

    log(pretend, verbose, "to remove:" if pretend else "removing:")
    for version in versions_to_remove:
        related_files = get_related_files(base_dir, version)
        log(pretend, verbose, '-', version)
        for path in related_files:
            log(pretend, verbose, '\t-', path)
            if pretend:
                continue
            try:
                shutil.move(path, removed_dir_path)
            except PermissionError as e:
                print(("\t  " if pretend or verbose else "") + "can't remove:", str(e))

    log(pretend, verbose, "to keep:" if pretend else "keeping:")
    for version in versions_to_keep:
        related_files = get_related_files(base_dir, version)
        log(pretend, verbose, '-', version)
        for path in related_files:
            log(pretend, verbose, '\t-', path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-dir", default="/boot")
    parser.add_argument("--pretend", action="store_true", default=False)
    parser.add_argument("--verbose", action="store_true", default=False)
    parser.add_argument("--keep-versions", type=int, default=DefaultArguments.KeepVersions, help=f"How many versions should be keeped (default: {DefaultArguments.KeepVersions})")
    args = parser.parse_args()

    cleanup(pathlib.PosixPath(args.base_dir), keep_versions=args.keep_versions, pretend=args.pretend,
            verbose=args.verbose)


if __name__ == "__main__":
    main()
