#include "linesegment.h"

bool LineSegment::hasPatternSet() {
    return mRegex.has_value();
}

void LineSegment::setPattern(regex r) {
    mRegex = r;
}

vector<Seg> LineSegment::formatLine(const string& line) {
    if (!mRegex.has_value())
        return {};

    smatch matches;
    if (!regex_search(line, matches, mRegex.value())) {
        return {};
    }

    vector<Seg> segs;
    for (size_t i = 1; i < matches.size(); i++) {
        segs.push_back({
            matches.position(i),
            matches.length(i)
        });
    }
    
    return segs;
}