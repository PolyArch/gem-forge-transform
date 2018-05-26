import os
import subprocess

for benchmark in os.listdir('.'):
	current_path = os.getcwd()
	l1_path = os.path.join('.', benchmark)
	if not os.path.isdir(l1_path):
		continue
	for subbenchmark in os.listdir(l1_path):
		l2_path = os.path.join(l1_path, subbenchmark)
		if not os.path.isdir(l2_path):
			continue
		os.chdir(l2_path)
		try:
			subprocess.check_call(['make', 'clean_all'])
		except Exception as e:
			print('Something wrong at {path}'.format(path=l2_path))
		os.chdir(current_path)


