#include "RadixSortPass.hpp"

void RadixSortPass::execute(RenderContext& renderContext)
{
    //TODO: dont like this solution, I would rather validCount be in the of them have a common struct they pass down
    unsigned int validCount = computeKeyValuesPre(renderContext); 
    sort(renderContext, validCount);
    gatherPost(renderContext, validCount);
}

unsigned int RadixSortPass::computeKeyValuesPre(RenderContext& renderContext)
{
#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::RADIX_SORT_KEYSVALUE, -1, "RADIX_SORT_KEYSVALUE");
#endif 
    glFinish();
    unsigned int validCount = 0;
    //Assume that if not 0 then we loaded a ply file
    //TODO: i am setting/reading this indirect buffer way too many times from the cpu
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderContext.drawIndirectBuffer);

    DrawElementsIndirectCommand* cmd = (DrawElementsIndirectCommand*)glMapBufferRange(
        GL_DRAW_INDIRECT_BUFFER, 0, sizeof(DrawElementsIndirectCommand), GL_MAP_READ_BIT
    );

    validCount = cmd->instanceCount;
    glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);


    // Transform Gaussian positions to view space and apply global sort
    glUseProgram(renderContext.shaderPrograms.radixSortPrepassProgram);
    glUtils::setUniformMat4(renderContext.shaderPrograms.radixSortPrepassProgram, "u_view", renderContext.viewMat);
    glUtils::setUniform1ui(renderContext.shaderPrograms.radixSortPrepassProgram, "u_count", validCount);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianBufferPostFiltering);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, renderContext.keysBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, renderContext.valuesBuffer);


    unsigned int threadGroup_xy = (validCount + 255) / 256;
    glDispatchCompute(threadGroup_xy, 1, 1);

    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
    return validCount;
}

void RadixSortPass::sort(RenderContext& renderContext, unsigned int validCount)
{
#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::RADIX_SORT_MAIN, -1, "RADIX_SORT_MAIN");
#endif 

    glu::RadixSort sorter;
    sorter(renderContext.keysBuffer, renderContext.valuesBuffer, validCount); //Syncronization is already handled within the sorter

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
};

void RadixSortPass::gatherPost(RenderContext& renderContext, unsigned int validCount)
{
#ifdef  _DEBUG
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, PassesDebugIDs::RADIX_SORT_GATHER, -1, "RADIX_SORT_GATHER");
#endif 

    glUseProgram(renderContext.shaderPrograms.radixSortGatherProgram);
    glUtils::setUniform1ui(renderContext.shaderPrograms.radixSortGatherProgram, "u_count", validCount);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.perQuadTransformationsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, renderContext.perQuadTransformationBufferSorted);  
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, renderContext.valuesBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, renderContext.drawIndirectBuffer);

    unsigned int threadGroup_xy = (validCount + 255) / 256;
    glDispatchCompute(threadGroup_xy, 1, 1);

    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
}
