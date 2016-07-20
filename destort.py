import cairo
import numpy
import math
import sys

infile = sys.argv[1]
outfile = infile + ".fixed.png"

ScreenWidth = 768
ScreenHeight = 480
ScreenMid = (ScreenWidth / 2, ScreenHeight / 2)

getLength = lambda (p1, p2): math.sqrt(math.pow(p1[0] - p2[0], 2) + math.pow(p1[1] - p2[1], 2))
getNorm = lambda (p): (p[0] / math.sqrt(p[0] * p[0] + p[1] * p[1]), p[1] / math.sqrt(p[0] * p[0] + p[1] * p[1]))
getDot = lambda p1, p2: p1[0] * p2[0] + p1[1] * p2[1]
getSub = lambda p1, p2: (p1[0] - p2[0], p1[1] - p2[1])
getAdd = lambda p1, p2: (p1[0] + p2[0], p1[1] + p2[1])
getScaled = lambda k, p: (k * p[0], k * p[1])
getRotated = lambda deg, p: (math.cos(math.radians(deg)) * p[0] - math.sin(math.radians(deg)) * p[1],
                             math.sin(math.radians(deg)) * p[0] + math.cos(math.radians(deg)) * p[1])
getAtan2 = lambda p: math.atan2(p[0], p[1])


data = None
with open(infile) as f:
    data = f.readlines()

    # 1. find quads
    # 2. figure out ideal quad targets (average or something)
    # 3. solve for transform


interestingPoints = []
for i in range(len(data) / 3):
    ps = map(lambda p: (float(p.split(" ")[0]), float(p.split(" ")[1])), data[3 * i: 3 * (i + 1)])

    lines = [(ps[0], ps[1]), (ps[1], ps[2]), (ps[2], ps[0])]
    
    

    for line in lines:
        if getLength(line) < 600:
            continue

        if getDot(getNorm(getSub(line[0], ScreenMid)), getNorm(getSub(line[1], ScreenMid))) < 0.8:
            continue

        interestingPoints.append(getNorm(getSub(line[0], ScreenMid)))

interestingPoints.sort(key=getAtan2)

interestingTargets = []
origin = interestingPoints[0]
srcArray = []
targArrayX = []
targArrayY = []
for i in range(len(interestingPoints)):
    interestingTargets.append(getRotated(i * -60.0, origin))
    print "{0} -> {1}".format(getScaled(100, interestingPoints[i]), getScaled(100, interestingTargets[i]))
    srcArray.append([interestingPoints[i][0], interestingPoints[i][1], 1])
    targArrayX.append([interestingTargets[i][0]])
    targArrayY.append([interestingTargets[i][1]])

srcArray = numpy.matrix(srcArray)
targArrayX = numpy.matrix(targArrayX)
targArrayY = numpy.matrix(targArrayY)

vars1 = numpy.linalg.solve(numpy.transpose(srcArray) * srcArray, numpy.transpose(srcArray) * targArrayX)
vars2 = numpy.linalg.solve(numpy.transpose(srcArray) * srcArray, numpy.transpose(srcArray) * targArrayY)

fixup = numpy.matrix([[vars1[0, 0], vars1[1, 0], vars1[2, 0]],
                      [vars2[0, 0], vars2[1, 0], vars2[2, 0]],
                      [0, 0, 1]])

print fixup


surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, ScreenWidth, ScreenHeight)
ctxt = cairo.Context(surf)
interestingPoints = []
for i in range(len(data) / 3):
    numMid = numpy.transpose(numpy.matrix([ScreenMid[0], ScreenMid[1], 0]))
    ps = map(lambda p: numpy.matrix([[float(p.split(" ")[0])], [float(p.split(" ")[1])], [1]]), data[3 * i: 3 * (i + 1)])
    ctxt.move_to(ps[0][0, 0], ps[0][1, 0])
    ctxt.line_to(ps[1][0, 0], ps[1][1, 0])
    ctxt.line_to(ps[2][0, 0], ps[2][1, 0])
    ctxt.close_path()
    ctxt.stroke()
surf.write_to_png(outfile + ".old.png")

surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, ScreenWidth, ScreenHeight)
ctxt = cairo.Context(surf)
interestingPoints = []
for i in range(len(data) / 3):
    numMid = numpy.transpose(numpy.matrix([ScreenMid[0], ScreenMid[1], 0]))
    ps = map(lambda p: numpy.matrix([[float(p.split(" ")[0])], [float(p.split(" ")[1])], [1]]), data[3 * i: 3 * (i + 1)])
    ps = map(lambda p: fixup * (p - numMid) + numMid, ps)
    ctxt.move_to(ps[0][0, 0], ps[0][1, 0])
    ctxt.line_to(ps[1][0, 0], ps[1][1, 0])
    ctxt.line_to(ps[2][0, 0], ps[2][1, 0])
    ctxt.close_path()
    ctxt.stroke()
surf.write_to_png(outfile)
