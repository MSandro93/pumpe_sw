import subprocess
import os

res = subprocess.check_output('git rev-parse HEAD', shell=True).decode('UTF-8').strip('\n')
file_path = os.getcwd() + "\\src\\Version.cpp"

f = open(file_path, "w")
f.truncate(0)

line = "char git_hash[] = \"" + res + "\";"

f.write(line)
f.flush()
f.close

print("Updated build hash to Version.cpp")