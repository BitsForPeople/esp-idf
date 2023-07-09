#!/usr/bin/env python
#
# ESP32 x509 certificate bundle generation utility
#
# Converts PEM and DER certificates to a custom bundle format which stores just the
# subject name and public key to reduce space
#
# The bundle will have the format: number of certificates; crt 1 subject name length; crt 1 public key length;
# crt 1 subject name; crt 1 public key; crt 2...
#
# SPDX-FileCopyrightText: 2018-2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from __future__ import with_statement

import argparse
import csv
import os
import re
import struct
import sys
from io import open

try:
    from cryptography import x509
    from cryptography.hazmat.backends import default_backend
    from cryptography.hazmat.primitives import serialization
except ImportError:
    print('The cryptography package is not installed.'
          'Please refer to the Get Started section of the ESP-IDF Programming Guide for '
          'setting up the required packages.')
    raise

ca_bundle_bin_file = 'x509_crt_bundle'

quiet = False


def status(msg):
    """ Print status message to stderr """
    if not quiet:
        critical(msg)


def critical(msg):
    """ Print critical message to stderr """
    sys.stderr.write('gen_crt_bundle.py: ')
    sys.stderr.write(msg)
    sys.stderr.write('\n')


class CertificateBundle:
    def __init__(self):
        self.certificates = []
        self.compressed_crts = []

    def add_from_path(self, crts_path):

        found = False
        for file_path in os.listdir(crts_path):
            found |= self.add_from_file(os.path.join(crts_path, file_path))

        if found is False:
            raise InputError('No valid x509 certificates found in %s' % crts_path)

    def add_from_file(self, file_path):
        try:
            if file_path.endswith('.pem'):
                status('Parsing certificates from %s' % file_path)
                with open(file_path, 'r', encoding='utf-8') as f:
                    crt_str = f.read()
                    self.add_from_pem(crt_str)
                    return True

            elif file_path.endswith('.der'):
                status('Parsing certificates from %s' % file_path)
                with open(file_path, 'rb') as f:
                    crt_str = f.read()
                    self.add_from_der(crt_str)
                    return True

        except ValueError:
            critical('Invalid certificate in %s' % file_path)
            raise InputError('Invalid certificate')

        return False

    def add_from_pem(self, crt_str):
        """ A single PEM file may have multiple certificates """

        crt = ''
        count = 0
        start = False

        for strg in crt_str.splitlines(True):
            if strg == '-----BEGIN CERTIFICATE-----\n' and start is False:
                crt = ''
                start = True
            elif strg == '-----END CERTIFICATE-----\n' and start is True:
                crt += strg + '\n'
                start = False
                self.certificates.append(x509.load_pem_x509_certificate(crt.encode(), default_backend()))
                count += 1
            if start is True:
                crt += strg

        if count == 0:
            raise InputError('No certificate found')

        status('Successfully added %d certificates' % count)

    def add_from_der(self, crt_str):
        self.certificates.append(x509.load_der_x509_certificate(crt_str, default_backend()))
        status('Successfully added 1 certificate')

    def create_bundle(self):
        # Sort certificates in order to do binary search when looking up certificates
        self.certificates = sorted(self.certificates, key=lambda cert: cert.subject.public_bytes(default_backend()))

        # List of offsets in bytes from the start of the bundle to each certificate inside
        offsets = []

        bundle = struct.pack('<H', len(self.certificates))

        for crt in self.certificates:
            """ Read the public key as DER format """
            pub_key = crt.public_key()
            pub_key_der = pub_key.public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)

            """ Read the subject name as DER format """
            sub_name_der = crt.subject.public_bytes(default_backend())

            name_len = len(sub_name_der)
            key_len = len(pub_key_der)
            len_data = struct.pack('<HH', name_len, key_len)

            # Certificate starts at this position in the bundle
            offsets.append(len(bundle))

            bundle += len_data
            bundle += sub_name_der
            bundle += pub_key_der

        # Store offsets for all certificates at the end of the bundle
        for off in offsets:
            bundle += struct.pack('<L',off)

        return bundle

    def add_with_filter(self, crts_path, filter_path):

        filter_set = set()
        with open(filter_path, 'r', encoding='utf-8') as f:
            csv_reader = csv.reader(f, delimiter=',')

            # Skip header
            next(csv_reader)
            for row in csv_reader:
                filter_set.add(row[1])

        status('Parsing certificates from %s' % crts_path)
        crt_str = []
        with open(crts_path, 'r', encoding='utf-8') as f:
            crt_str = f.read()

            # Split all certs into a list of (name, certificate string) tuples
            pem_crts = re.findall(r'(^.+?)\n(=+\n[\s\S]+?END CERTIFICATE-----\n)', crt_str, re.MULTILINE)

            filtered_crts = ''
            for name, crt in pem_crts:
                if name in filter_set:
                    filtered_crts += crt

        self.add_from_pem(filtered_crts)


class InputError(RuntimeError):
    def __init__(self, e):
        super(InputError, self).__init__(e)


def main():
    global quiet

    parser = argparse.ArgumentParser(description='ESP-IDF x509 certificate bundle utility')

    parser.add_argument('--quiet', '-q', help="Don't print non-critical status messages to stderr", action='store_true')
    parser.add_argument('--input', '-i', nargs='+', required=True,
                        help='Paths to the custom certificate folders or files to parse, parses all .pem or .der files')
    parser.add_argument('--output', '-o', default=ca_bundle_bin_file,
                        help='Name of the output file (default: %s), or - for stdout' % ca_bundle_bin_file)
    parser.add_argument('--arr', '-a', help='Output result as comma-separated list of byte values suitable for array initialization', action='store_true')
    parser.add_argument('--dec', '-d', help='Output byte values in decimal instead of hex; only valid if used with -a', action='store_true')
    parser.add_argument('--filter', '-f', help='Path to CSV-file where the second columns contains the name of the certificates \
                        that should be included from cacrt_all.pem')

    args = parser.parse_args()

    quiet = args.quiet

    bundle = CertificateBundle()

    for path in args.input:
        if os.path.isfile(path):
            if os.path.basename(path) == 'cacrt_all.pem' and args.filter:
                bundle.add_with_filter(path, args.filter)
            else:
                bundle.add_from_file(path)
        elif os.path.isdir(path):
            bundle.add_from_path(path)
        else:
            raise InputError('Invalid --input=%s, is neither file nor folder' % args.input)

    status('Successfully added %d certificates in total' % len(bundle.certificates))

    crt_bundle = bundle.create_bundle()

    # By default, the output is the crt_bundle's binary data
    result = crt_bundle

    # Transform binary to array initialization if requested
    if args.arr:
        # Default: hex
        fmt = '0x{0:02x}, '
        if args.dec:
            fmt = '{0:d}, '
        cnt = 0
        result = ''
        for b in crt_bundle:
            result += fmt.format(b)
            cnt += 1
            if (cnt % 20) == 0:
                result += '\n'
        # Remove trailing comma
        if result.endswith(', '):
            result = result[:len(result) - 2]
        result = bytes(result, 'utf-8')

    # Output to file or stdout as needed
    if args.output == '-':
        sys.stdout.buffer.write(result)
    else:
        # if os.path.isfile(args.output):
        #     os.remove(args.output)
        with open(args.output, 'wb') as f:
            f.write(result)


if __name__ == '__main__':
    try:
        main()
    except InputError as e:
        print(e)
        sys.exit(2)
