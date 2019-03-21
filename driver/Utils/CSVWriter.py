"""
A more convenient csv writer.
"""

import csv


class CSVWriter:
    def __init__(self, fn, fields):
        self.stream = open(fn, 'w')
        self.stream.write(','.join([str(f) for f in fields]))
        self.stream.write('\n')
        self.writer = csv.writer(self.stream)

    def close(self):
        self.stream.close()

    def writerow(self, row):
        self.writer.writerow(row)
