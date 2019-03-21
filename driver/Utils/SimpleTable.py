

class SimpleTable:
    def __init__(self, header, columns):
        self.header = header
        self.columns = columns
        self.headers = list()
        self.datas = list()

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

    def add_geomean_row(self, column):
        column_name = self.columns[column]
        data = [row[column] for row in self.datas]
        geomean_name = 'geomean({x})'.format(x=column_name)
        self.headers.append(geomean_name)
        geomean_row = list()
        for c in xrange(len(self.datas[0])):
            if c == column:
                geomean_row.append(self.compute_geomean(data))
            else:
                geomean_row.append('/')
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
        table = prettytable.PrettyTable([self.header] + self.columns)
        for i in xrange(len(self.headers)):
            table.add_row([self.headers[i]] + self.datas[i])
        table.float_format = '.2e'
        return table.get_string()
