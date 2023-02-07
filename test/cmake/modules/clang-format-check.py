#!/usr/bin/env python3
#
# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
# Copyright (C) 2018-2019  UAVCAN Development Team  <uavcan.org>
# This software is distributed under the terms of the MIT License.
#
"""
    Uses clang-format to check style and return pass/fail. No source code
    is modified.
"""

import argparse
import logging
import sys
import subprocess
import glob

import xml.dom.minidom as xml
from xml.dom.minidom import Document, Node

from typing import List

__version__ = '1.0.0'

__license__ = 'MIT'


def _run(args: argparse.Namespace) -> int:
    """
    Run clang format and return the appropriate result code.
    """

    glob_pattern = getattr(args, 'glob-pattern')
    glob_results = glob.glob(glob_pattern, recursive=True)

    if len(glob_results) == 0:
        logging.warning('No files were found for glob-pattern {}'.format(glob_pattern))
        return -1

    result = 0  # type: int

    for glob_result in glob_results:

        completed = subprocess.run([args.clang_format_path, '-fallback-style=none', '-style=file', '-output-replacements-xml', glob_result],
                                   check=True,
                                   stdout=subprocess.PIPE)  # type: subprocess.CompletedProcess

        replacement_doc = completed.stdout
        replacements = []  # type: List[str]
        try:
            replacement_dom = xml.parseString(replacement_doc)  # type: Document
            for replacments_element in replacement_dom.childNodes:
                if replacments_element.nodeName == 'replacements':
                    for replacement_element in replacments_element.childNodes:
                        if replacement_element.nodeName == 'replacement':
                            replacements_text = ''
                            for text_element in replacement_element.childNodes:
                                if text_element.nodeType == Node.TEXT_NODE:
                                    replacements_text += text_element.data
                            replacements.append('length={}, offset={}, replacement="{}"'.format(replacement_element.getAttribute('length'),
                                                                                                replacement_element.getAttribute('offset'),
                                                                                                replacements_text.replace('\n', '&#10;')))
        except Exception as e:
            logging.warning(e)

        if len(replacements) > 0:
            result = -1
            do_clang_format = [ args.clang_format_path, '-style=file', '-i', glob_result]
            logging.warning('File {} has {} formatting errors. Run clang-format:\n{}'.format(
                glob_result,
                len(replacements),
                ' '.join(do_clang_format)))

    return result


def _make_parser() -> argparse.ArgumentParser:
    """
        Defines the command-line interface.
    """

    epilog = '''**Example Usage**::

    %(prog)s path/to/libcyphal/libcyphal/**/*.hpp

----
'''

    parser = argparse.ArgumentParser(
        description='Use clang-format to check compliance with a style guide.',
        epilog=epilog,
        formatter_class=argparse.RawTextHelpFormatter)

    parser.add_argument('--clang-format-path', default='clang-format', help='The path and executable name to use when invoking clang-format.')

    parser.add_argument('--verbose', '-v', action='count',
                        help='verbosity level (-v, -vv)')

    parser.add_argument('--version', action='version', version=__version__)

    parser.add_argument('glob-pattern',
                        help='Unix-style pathname pattern to enumerate files to format')

    return parser


def main() -> int:
    """
        Main entry point for this program.
    """

    #
    # Parse the command-line arguments.
    #
    parser = _make_parser()
    args = parser.parse_args()

    #
    # Setup Python logging.
    #
    fmt = '%(message)s'
    level = {0: logging.WARNING, 1: logging.INFO,
             2: logging.DEBUG}.get(args.verbose or 0, logging.DEBUG)
    logging.basicConfig(stream=sys.stderr, level=level, format=fmt)

    logging.info('Running %s using sys.prefix: %s',
                 parser.prog, sys.prefix)

    #
    # Run the core program logic
    #
    return _run(args)


if __name__ == "__main__":
    sys.exit(main())
