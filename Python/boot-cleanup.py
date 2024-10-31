#! /usr/bin/env python3

import argparse
import glob
import os
import pathlib
import re
import shutil
import subprocess


class DefaultArguments:
    KeepVersions = 3


def natural_sort_key(s, _nsre=re.compile('([0-9]+)')):
    return [int(text) if text.isdigit() else text.lower() for text in _nsre.split(s)]


def execute_text_command(cmd: list[str], exception_on_error: bool = True) -> str:
    out = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    stdout, stderr = out.communicate()
    if exception_on_error and out.returncode != 0:
        raise Exception(f"Command failed with exit code {out.returncode}: " + ", ".join(cmd))
    return stdout.decode('utf-8')


def get_current_kernel_version() -> str:
    return execute_text_command(["/usr/bin/uname", "-r"]).splitlines()[0]


def get_related_files(base_dir: pathlib.Path, version_postfix: str):
    pattern = str(base_dir.joinpath("*" + version_postfix))
    return glob.glob(pattern)


def log(pretend: bool, verbose: bool, *args):
    if pretend or verbose:
        print(*args)


def cleanup(base_dir: pathlib.Path, ignore_current_version: bool, ignore_current_old_version: bool, keep_versions: int, pretend: bool, verbose: bool):
    assert (keep_versions >= 0)
    versions = set()
    for path in glob.glob(str(base_dir.joinpath("vmlinuz-*")), recursive=False):
        versions.add(re.match(r".*vmlinuz-(.*)", path)[1])

    old_versions = set(filter(lambda version: 'old' in str(version).lower(), versions))

    if not pretend:
        removed_dir_path = base_dir.joinpath("removed")
        if not os.path.isdir(removed_dir_path):
            os.mkdir(removed_dir_path)

    current_kernel_version = get_current_kernel_version()
    versions_similar_to_current_kernel = set(filter(lambda version: current_kernel_version in str(version), versions))
    old_versions_similar_to_current_kernel = set(filter(lambda version: 'old' in str(version).lower(), versions_similar_to_current_kernel))
    versions_similar_to_current_kernel.difference_update(old_versions_similar_to_current_kernel)

    if 0 < len(versions_similar_to_current_kernel):
        log(pretend, verbose, f"versions looking like currently running kernel ({current_kernel_version}):")
        for version in sorted(versions_similar_to_current_kernel):
            log(pretend, verbose, '-', version)
    if 0 < len(old_versions_similar_to_current_kernel):
        log(pretend, verbose, f"old versions looking like currently running kernel ({current_kernel_version}):")
        for version in sorted(old_versions_similar_to_current_kernel):
            log(pretend, verbose, '-', version)

    if ignore_current_version:
        versions_similar_to_current_kernel.clear()

    if ignore_current_version or ignore_current_old_version:
        old_versions_similar_to_current_kernel.clear()

    old_versions.difference_update(old_versions_similar_to_current_kernel)
    other_versions = versions - versions_similar_to_current_kernel - old_versions_similar_to_current_kernel - old_versions

    versions_in_order = (sorted(old_versions, key=natural_sort_key)
                         + sorted(other_versions, key=natural_sort_key)
                         + sorted(old_versions_similar_to_current_kernel, key=natural_sort_key)
                         + sorted(versions_similar_to_current_kernel, key=natural_sort_key))

    assert len(versions) == len(versions_in_order)

    keep_versions = max(0, keep_versions, max(0, keep_versions - 1) + len(versions_similar_to_current_kernel) + len(old_versions_similar_to_current_kernel))
    versions_cut_line = max(0, len(versions_in_order) - keep_versions)

    versions_to_remove = versions_in_order[0:versions_cut_line]
    versions_to_keep = versions_in_order[versions_cut_line:]

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
    parser.add_argument("-d", "--base-dir", default="/boot")
    parser.add_argument("-c", "--ignore-current-version", action="store_true", dest='ignore_current_version',
                        help="Don't try to check and preserve currently running kernel")
    parser.add_argument("-o", "--ignore-current-old-version", action="store_true", dest='ignore_current_old_version',
                        help="Don't try to check and preserve old version of currently running kernel")
    parser.add_argument("-k", "--keep-versions", type=int, default=DefaultArguments.KeepVersions,
                        help=f"How many versions should be kept (default: {DefaultArguments.KeepVersions})")
    parser.add_argument("-p", "--pretend", action="store_true")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    cleanup(pathlib.PosixPath(args.base_dir), ignore_current_version=args.ignore_current_version,
            ignore_current_old_version=args.ignore_current_old_version, keep_versions=args.keep_versions, pretend=args.pretend, verbose=args.verbose)


if __name__ == "__main__":
    main()
