#!/usr/bin/evn python
import os
import sys
import re
import pandas
import io

file_mapping_test = {
    '001': ('without', 'test'),
    '002': ('modified', 'test'),
    '003': ('normal', 'test'),
}

file_mapping_traing = {
    '004': ('without', 'train'),
    '005': ('modified', 'train'),
    '006': ('normal', 'train'),
}

benchmarks = ['358.botsalgn', '372.smithwa', '376.kdtree']

for mapping, file_mapping in [('test', file_mapping_test), ('train', file_mapping_traing)]:
    csv_string = []
    for experiment_dir in sys.argv[1:]:
        if not os.path.isdir(experiment_dir):
            print(f"Error: {experiment_dir} is not a directory.")
            exit(1)

        # iterate over file_mapping
        for i, (experiment_type, input_size) in file_mapping.items():
            with open(os.path.join(experiment_dir, f"OMPG2012.{i}.{input_size}.csv"), 'r') as f:
                lines = f.readlines()
                for benchmark in benchmarks:
                    m = re.findall(rf"{benchmark},[0-9]+,.*iteration.*", ''.join(lines))
                    for result in m:
                        csv_string.append(result + f",{experiment_type}")
    # read CSV from string in pandas dataframe
    csv_string = ['Benchmark,"Base # Threads","Est. Base Run Time","Est. Base Ratio","Base Selected","Base Status","Peak # Threads","Est. Peak Run Time","Est. Peak Ratio","Peak Selected","Peak Status",Description,Run Mode'] + csv_string  # Add header
    csv_string = '\n'.join(csv_string)  # Join into a single string
    # Convert to pandas DataFrame
    csv_string = io.StringIO(csv_string)  # Use StringIO to read from string
    pd = pandas.read_csv(csv_string)
    # select last column pandas
    pd = pd.iloc[:, [0, -1, 1, 2, 5]].sort_values(by=['Benchmark', 'Base # Threads'])  # Select specific columns
    pd.to_csv(f'spec_{mapping}.csv', header=True, index=False)  # Save to CSV without index and header
