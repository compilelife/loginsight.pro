#include "linesegment.h"

bool LineSegment::hasPatternSet() {
    return mRegex.has_value();
}

void LineSegment::setPattern(regex r) {
    mRegex = r;
}

void LineSegment::setSegments(vector<Segment>&& segmentsInGroupOrder) {
    mSegments = segmentsInGroupOrder;
}

vector<LineSeg> LineSegment::formatLine(const string& line) {
    if (!mRegex.has_value())
        return {};

    smatch matches;
    if (!regex_search(line, matches, mRegex.value())) {
        return {};
    }

    vector<LineSeg> segs;
    for (size_t i = 1; i < matches.size(); i++) {
        segs.push_back(LineSeg{
            (LineCharI)matches.position(i),
            (LineCharI)matches.length(i)
        });
    }
    
    return segs;
}

string LineSegment::makeDebugPrint(
        string_view line, 
        const vector<LineSeg>& segs) {
    string buf;
    for (size_t i = 0; i < segs.size(); i++) {
        auto name = mSegments[i].name;
        auto [offset, length] = segs[i];
        buf.append(name+": "+string(line.substr(offset, length))+"\n");
    }
    return buf;
}