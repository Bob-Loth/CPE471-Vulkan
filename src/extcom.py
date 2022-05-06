# run this to extract the comments of the local directory's c++ files.

from comment_parser import comment_parser as p
import glob


for f in glob.glob("./*.cc") + glob.glob("./*.h"):
    print(f)
    print('\n')
    for com in p.extract_comments(filename=f, mime="text/x-c++"):
        print(com)
        
    print()