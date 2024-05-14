#version 430 core
layout(vertices = 3) out;

uniform float minTriangleArea;
uniform float maxTriangleArea;
uniform float medianTriangleArea;
uniform float longestEdge;
uniform float shortestEdge;

in VS_OUT {
    vec3 position;
} tcs_in[];

out TCS_OUT {
    vec3 position;
} tcs_out[];

float calcTriangleArea(vec3 p1, vec3 p2, vec3 p3)
{
    float a = length(p1 - p2);
    float b = length(p1 - p3);
    float c = length(p2 - p3);

    float p = (a + b + c) * .5;
        
    return sqrt(p * (p - a) * (p - b) * (p - c));
}

void main() { 

    float elongationWeight = 0.9;  // More weight on elongation
    float areaWeight = 0.1; 

    float edge1 = length(tcs_in[0].position - tcs_in[1].position);
    float edge2 = length(tcs_in[1].position - tcs_in[2].position);
    float edge3 = length(tcs_in[2].position - tcs_in[0].position);

    float perimeter = edge1 + edge2 + edge3;
    float ratio1 = edge1 / perimeter;
    float ratio2 = edge2 / perimeter;
    float ratio3 = edge3 / perimeter;
    
    float elongationThreshold = 0.45; // TBD

    float maxLength = max(edge1, max(edge2, edge3));

    float elongationFactor = maxLength / perimeter;

    float baseLevel = 1.0; // Minimum tessellation level
    float highLevel = 10.0; // Maximum tessellation level for the longest edge

    // Calculate base tessellation level based on area
    float area = calcTriangleArea(tcs_in[0].position, tcs_in[1].position, tcs_in[2].position);
    float normalizedArea = (area - minTriangleArea) / (maxTriangleArea - minTriangleArea);

    // Evaluate elongation
    bool elongated1 = ratio1 > elongationThreshold; 
    bool elongated2 = ratio2 > elongationThreshold; 
    bool elongated3 = ratio3 > elongationThreshold; 

    float fac2 = mix(baseLevel, highLevel, mix(normalizedArea, 0.0, areaWeight));

    // Assign tessellation levels based on elongation
    int tessLevel1 = int(elongated1 ? mix(baseLevel, highLevel, 1 - mix(normalizedArea, 1.0, areaWeight)) : fac2);
    int tessLevel2 = int(elongated2 ? mix(baseLevel, highLevel, 1 - mix(normalizedArea, 1.0, areaWeight)) : fac2);
    int tessLevel3 = int(elongated3 ? mix(baseLevel, highLevel, 1 - mix(normalizedArea, 1.0, areaWeight)) : fac2);

    // Set the tessellation levels
    gl_TessLevelOuter[0] = tessLevel1;
    gl_TessLevelOuter[1] = tessLevel2;
    gl_TessLevelOuter[2] = tessLevel3;

    // Set inner tessellation level based on the most elongated case
    if (elongated1 || elongated2 || elongated3) {
        gl_TessLevelInner[0] = max(tessLevel1, max(tessLevel2, tessLevel3));
    } else {
        gl_TessLevelInner[0] = fac2;
    }


    tcs_out[gl_InvocationID].position = tcs_in[gl_InvocationID].position;
}
