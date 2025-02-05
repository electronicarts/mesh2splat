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
    uint32_t validCount = 0;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, renderContext.atomicCounterBuffer);
    glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(uint32_t), &validCount);

    // Transform Gaussian positions to view space and apply global sort
    glUseProgram(renderContext.shaderPrograms.radixSortPrepassProgram);
    glUtils::setUniformMat4(renderContext.shaderPrograms.radixSortPrepassProgram, "u_view", renderContext.viewMat);
    glUtils::setUniform1ui(renderContext.shaderPrograms.radixSortPrepassProgram, "u_count", validCount);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianDepthPostFiltering);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, renderContext.keysBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, renderContext.valuesBuffer);

    unsigned int threadsPerGroup = 16 * 16; 
    unsigned int totalGroupsNeeded = (validCount + threadsPerGroup - 1) / threadsPerGroup;
    unsigned int groupsX = (unsigned int)ceil(sqrt((float)totalGroupsNeeded));
    unsigned int groupsY = (totalGroupsNeeded + groupsX - 1) / groupsX; 
    glDispatchCompute(groupsX, groupsY, 1);

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

    //Abstract this
    unsigned int threadsPerGroup = 16 * 16; 
    unsigned int totalGroupsNeeded = (validCount + threadsPerGroup - 1) / threadsPerGroup;
    unsigned int groupsX = (unsigned int)ceil(sqrt((float)totalGroupsNeeded));
    unsigned int groupsY = (totalGroupsNeeded + groupsX - 1) / groupsX; 
    glDispatchCompute(groupsX, groupsY, 1);

    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

#ifdef  _DEBUG
    glPopDebugGroup();
#endif 
}
