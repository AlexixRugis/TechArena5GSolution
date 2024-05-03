from PIL import Image, ImageDraw, ImageFont
import subprocess
from sys import path
import random
import colorutils

EXEC_PATH = 'x64/Release/Project.exe'
OUTPUT_PATH = 'Visualization'

class Interval:
    def __init__(self, start, end, users):
        self.start = start
        self.end = end
        self.users = users
        
class TestCase:
    def __init__(self, number, accuracy, intervals):
        self.number = number
        self.accuracy = accuracy
        self.intervals = intervals
    
def getline(stdout):
    return stdout.readline().decode('utf-8')
    
def parse_test(stdout):
    intervals_list = list()
    
    header = getline(stdout)
    if not header:
        return None
    
    header = header.split()
    test_case = header[1]
    accuracy = header[3]
    
    intervals = getline(stdout)
    if not intervals:
        return None
    intervals = int(intervals.split()[1])
    
    blank_line = getline(stdout)
    if not blank_line: return None
    
    for i in range(intervals):
        int_data = getline(stdout)
        if not int_data: return None
        int_data = int_data.split()
        start = int(int_data[0])
        end = int(int_data[1])
        users = sorted(map(int, int_data[2:]))
        intervals_list.append(Interval(start, end, users))
        
    intervals_list.sort(key=lambda x: x.start)
    test_case = TestCase(test_case, accuracy, intervals_list)
    
    return test_case
    
    
def render_testcase(testcase: TestCase, realTests):
    testData = realTests[int(testcase.number)-1]
    
    PADDING = 10
    BTW_BLOCK_PADDING = 5
    BLOCK_WIDTH_SCALE = 10
    BLOCK_HEIGHT = 20
    
    maxUserId = testData['N']
    maxUsers = testData['L']
        
    userColors = [colorutils.Color(hsv=((180 * i / maxUserId), 0.5, 1)).hex for i in range(maxUserId)]
    random.shuffle(userColors)
    
    userEnds = {}
    for intvl in testcase.intervals:
        for us in intvl.users:
            if not us in userEnds:
                userEnds[us] = intvl.start + testData['users'][us][0]
    
    start = 0
    end = testData['M']
    
    intervals_count = len(testcase.intervals)
        
    w, h = 2*PADDING + (end - start)*BLOCK_WIDTH_SCALE + (intervals_count-1)*BTW_BLOCK_PADDING, 2*PADDING + (maxUsers+1)*BLOCK_HEIGHT + maxUsers*BTW_BLOCK_PADDING
    
    font = ImageFont.truetype("Fonts/Roboto-Medium.ttf", 16)
    img = Image.new("RGB", (w, h)) 
    img1 = ImageDraw.Draw(img)   
    
    img1.text((PADDING, PADDING), f'Test {testcase.number} Accuracy {testcase.accuracy}', (255,255,255),font=font)

    for j, intvl in enumerate(testcase.intervals):
        x0 = PADDING + j*BTW_BLOCK_PADDING + intvl.start * BLOCK_WIDTH_SCALE
        x1 = PADDING + j*BTW_BLOCK_PADDING + intvl.end * BLOCK_WIDTH_SCALE
        
        for i, us in enumerate(intvl.users):
            y0 = PADDING + (i+1) * (BLOCK_HEIGHT + BTW_BLOCK_PADDING)
            y1 = y0 + BLOCK_HEIGHT
            col = userColors[us]
            ux1 = PADDING + j*BTW_BLOCK_PADDING + userEnds[us] * BLOCK_WIDTH_SCALE
            ux1 = min(ux1, x1)
            img1.rectangle(((x0, y0), (ux1, y1)), fill=col)
            img1.text((x0 + BTW_BLOCK_PADDING, y0),str(us),(0,0,0),font=font)
    
    img.save(f'Visualization/{testcase.number}.jpg')
    
def load_real_tests():
    realTests = []
    with open('open.txt', 'r') as f:
        f.readline()
        lines = []
        for i in f.readlines():
            data = list(map(int, i.split()))
            lines.append(data)
            
        index = 0
        while (index < len(lines)):
            header = lines[index]
            index += 1
            test = {}
            test['N'], test['M'], test['K'], test['J'], test['L'] = header[0],header[1],header[2],header[3],header[4]
            test['reserved'] = []
            for i in range(test['K']):
                test['reserved'].append(lines[index])
                index+=1
            test['users'] = []
            for i in range(test['N']):
                test['users'].append(lines[index])
                index += 1
            realTests.append(test)
    return realTests

def main():
    realTests = load_real_tests()    
    
    p = subprocess.Popen(EXEC_PATH, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    while True:
        testcase = parse_test(p.stdout)
        if not testcase:
            break
        
        render_testcase(testcase, realTests)
        
if __name__ == '__main__':
    main()