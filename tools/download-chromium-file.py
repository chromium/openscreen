#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script is used to download a file from the Chromium repository.

It's equivalent to using curl to download the file, and is intended to be ran
as a gclient hook.
"""

import argparse
import base64
import os
import random
import sys
import time
import urllib.error
import urllib.request

GITILES_URL_TEMPLATE = 'https://chromium.googlesource.com/chromium/src/+/{}/{}?format=TEXT'

# Status codes that may indicate temporary server unavailability or rate
# limiting.
RETRY_STATUS_CODES = {429, 500, 502, 503, 504}
MAX_ATTEMPTS = 5
INITIAL_BACKOFF_SECONDS = 2.0
TIMEOUT_SECONDS = 30.0


def download_file(revision: str,
                  path: str,
                  output_path: str,
                  max_attempts: int = MAX_ATTEMPTS,
                  initial_backoff: float = INITIAL_BACKOFF_SECONDS) -> bool:
    url = GITILES_URL_TEMPLATE.format(revision or 'main', path)
    print(f'  -> Downloading from "{url}" to "{output_path}"...')
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})

    for attempt in range(max_attempts):
        try:
            with urllib.request.urlopen(req, timeout=TIMEOUT_SECONDS) as resp:
                content = base64.b64decode(resp.read())
            dir_path = os.path.dirname(os.path.abspath(output_path))
            os.makedirs(dir_path, exist_ok=True)
            with open(output_path, 'wb') as f:
                f.write(content)
            return True
        except urllib.error.HTTPError as e:
            if e.code in RETRY_STATUS_CODES and attempt < max_attempts - 1:
                backoff = initial_backoff * (2**attempt) + random.uniform(0, 1)
                print(
                    f'WARNING: Failed to download {url} (HTTP {e.code}). '
                    f'Retrying in {backoff:.1f}s '
                    f'(attempt {attempt + 1}/{max_attempts})...',
                    file=sys.stderr)
                time.sleep(backoff)
                continue
            print(f'ERROR: Failed to download {url}: {e}', file=sys.stderr)
            return False
        except (urllib.error.URLError, TimeoutError) as e:
            if attempt < max_attempts - 1:
                backoff = initial_backoff * (2**attempt) + random.uniform(0, 1)
                reason = getattr(e, 'reason', str(e))
                print(
                    f'WARNING: Failed to download {url} ({reason}). '
                    f'Retrying in {backoff:.1f}s '
                    f'(attempt {attempt + 1}/{max_attempts})...',
                    file=sys.stderr)
                time.sleep(backoff)
                continue
            print(f'ERROR: Failed to download {url}: {e}', file=sys.stderr)
            return False
    return False


def main():
    parser = argparse.ArgumentParser(
        description='Download a file from the Chromium repository')
    parser.add_argument('--output',
                        required=True,
                        help='path to file to create/overwrite')
    parser.add_argument('--revision',
                        required=True,
                        help='revision to download')
    parser.add_argument('--path',
                        required=True,
                        help='path within the Chromium repository')
    args = parser.parse_args()

    return 0 if download_file(args.revision, args.path, args.output) else 1


if __name__ == '__main__':
    sys.exit(main())
