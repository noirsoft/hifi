//
//  StdDev.h
//  libraries/shared/src
//
//  Created by Philip Rosedale on 3/12/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_StdDev_h
#define hifi_StdDev_h

class StDev {
    public:
        StDev();
        void reset();
        void addValue(float v);
        float getAverage() const;
        float getStDev() const;
        int getSamples() const { return sampleCount; }
    private:
        float * data;
        int sampleCount;
};

#endif // hifi_StdDev_h
