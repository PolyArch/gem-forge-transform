

class SimpleTable:
    def __init__(self, header, columns, compress_columns=False):
        self.header = header
        self.headers = list()
        self.datas = list()
        self.compress_columns = compress_columns
        self.columns = columns

    def add_row(self, header, data):
        self.headers.append(header)
        self.datas.append(data)

    def add_ratio_column(self, dividend, divisor):
        dividend_name = self.columns[dividend]
        divisor_name = self.columns[divisor]
        for row in self.datas:
            dividend_value = row[dividend]
            divisor_value = row[divisor]
            row.append(dividend_value / divisor_value)
        division_name = '{x}/{y}'.format(x=dividend_name, y=divisor_name)
        self.columns.append(division_name)

    def add_geomean_row(self):
        self.headers.append('geomean')
        geomean_row = list()
        for c in xrange(len(self.datas[0])):
            geomean_row.append(self.compute_geomean(
                [row[c] for row in self.datas]))
        self.datas.append(geomean_row)

    def compute_geomean(self, data):
        p = 1.0
        n = 0.0
        for v in data:
            if isinstance(v, float):
                p *= v
                n += 1.0
        return p ** (1.0 / n)

    def __str__(self):
        import prettytable
        columns = self.columns
        column_legend = ''
        if self.compress_columns:
            compressed_columns = list()
            for i in range(len(self.columns)):
                compressed_column = chr(ord('A') + i)
                compressed_columns.append(compressed_column)
                column_legend += '{compressed_column}={column}\n'.format(
                    compressed_column=compressed_column, column=self.columns[i])
            columns = compressed_columns
        table = prettytable.PrettyTable([self.header] + columns)
        for i in xrange(len(self.headers)):
            table.add_row([self.headers[i]] + self.datas[i])
        table.float_format = '.2e'
        return column_legend + table.get_string()
