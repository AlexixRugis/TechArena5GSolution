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
    header = getline(stdout)
    print(header)
    if not header:
        return None
    
    header = header.split()
    if len(header) != 2:
        return None
    
    test_case = header[1]
    accuracy = ''
    
    test_cases_list = []
    while True:
        intervals = getline(stdout)
        if not intervals:
            return None
        intervals = intervals.split()
        if intervals[0] == 'Filled:':
            accuracy = intervals[1]
            break
            
        intervals = int(intervals[1])
        
        blank_line = getline(stdout)
        if not blank_line: return None
        
        intervals_list = list()
        for i in range(intervals):
            int_data = getline(stdout)
            if not int_data: return None
            int_data = int_data.split()
            start = int(int_data[0])
            end = int(int_data[1])
            users = sorted(map(int, int_data[2:]))
            intervals_list.append(Interval(start, end, users))
            
        intervals_list.sort(key=lambda x: x.start)
        test_cases_list.append(intervals_list)
    
    output = []
    for l in test_cases_list:
        output.append(TestCase(test_case, accuracy, l))
    
    return output
    
    
def render_testcase(testcase: TestCase, index: int, realTests):
    testData = realTests[int(testcase.number)-1]
    
    PADDING = 10
    BTW_BLOCK_PADDING = 5
    BLOCK_WIDTH_SCALE = 10
    BLOCK_HEIGHT = 20
    
    maxUserId = testData['N']
    maxUsers = testData['L']
        
    userColors = [colorutils.Color(hsv=((180 * i / maxUserId), 0.5, 1)).hex for i in range(maxUserId)]
    random.seed(17239)
    random.shuffle(userColors)
    
    userStarts = {}
    userEnds = {}
    realEnds = {}
    for intvl in testcase.intervals:
        for us in intvl.users:
            realEnds[us] = intvl.end
            if not us in userEnds:
                userEnds[us] = intvl.start + testData['users'][us][0]
            if not us in userStarts:
                userStarts[us] = intvl.start
    
    start = 0
    end = testData['M']
    
    intervals_count = len(testcase.intervals)
        
    w, h = 2*PADDING + (end - start)*BLOCK_WIDTH_SCALE + (intervals_count-1)*BTW_BLOCK_PADDING, 2*PADDING + (maxUsers+1)*BLOCK_HEIGHT + maxUsers*BTW_BLOCK_PADDING
    
    font = ImageFont.truetype("Fonts/Roboto-Medium.ttf", 16)
    img = Image.new("RGB", (w, h)) 
    img1 = ImageDraw.Draw(img)   
    
    img1.text((PADDING, PADDING), f'Test {testcase.number} Accuracy {testcase.accuracy} Inserted {len(userStarts)} of {maxUserId}', (255,255,255),font=font)

    for j, intvl in enumerate(testcase.intervals):
        x0 = PADDING + j*BTW_BLOCK_PADDING + intvl.start * BLOCK_WIDTH_SCALE
        x1 = PADDING + j*BTW_BLOCK_PADDING + intvl.end * BLOCK_WIDTH_SCALE
        
        for i in range(testData['L']):
            y0 = PADDING + (i+1) * (BLOCK_HEIGHT + BTW_BLOCK_PADDING)
            y1 = y0 + BLOCK_HEIGHT
            img1.rectangle(((x0, y0), (x1, y1)), fill='#606060')
        
        for i, us in enumerate(intvl.users):
            y0 = PADDING + (i+1) * (BLOCK_HEIGHT + BTW_BLOCK_PADDING)
            y1 = y0 + BLOCK_HEIGHT
            col = userColors[us]
            ux1 = PADDING + j*BTW_BLOCK_PADDING + userEnds[us] * BLOCK_WIDTH_SCALE
            ux1 = min(ux1, x1)
            
            fill = (realEnds[us] - userStarts[us]) / testData['users'][us][0]
            
            img1.rectangle(((x0, y0), (ux1, y1)), fill=col)
            img1.text((x0 + BTW_BLOCK_PADDING, y0),f'{us} {round(fill, 2)}',(0,0,0),font=font)
    
    img.save(f'Visualization/{testcase.number}_{index}.jpg')
    
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
        testcases = parse_test(p.stdout)
        if not testcases:
            break
        
        for i, case in enumerate(testcases):
            render_testcase(case, i, realTests)
        
if __name__ == '__main__':
    main()