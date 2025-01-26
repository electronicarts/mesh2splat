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

     glBindBuffer(GL_DRAW_INDIRECT_BUFFER, renderContext.drawIndirectBuffer);

    DrawArraysIndirectCommand* cmd = (DrawArraysIndirectCommand*)glMapBufferRange(
        GL_DRAW_INDIRECT_BUFFER, 0, sizeof(DrawArraysIndirectCommand), GL_MAP_READ_BIT
    );

    unsigned int validCount = cmd->instanceCount;
    glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);

    // Transform Gaussian positions to view space and apply global sort
    glUseProgram(renderContext.shaderPrograms.radixSortPrepassProgram);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianBuffer);
    setUniformMat4(renderContext.shaderPrograms.radixSortPrepassProgram, "u_view", renderContext.viewMat);
    setUniform1ui(renderContext.shaderPrograms.radixSortPrepassProgram, "u_count", validCount);
    setupKeysBufferSsbo(validCount, renderContext.keysBuffer, 1);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.keysBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, renderContext.keysBuffer);
    setupValuesBufferSsbo(validCount, renderContext.valuesBuffer, 2);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.valuesBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, renderContext.valuesBuffer);
    unsigned int threadGroup_xy = (validCount + 255) / 256;
    glDispatchCompute(threadGroup_xy, 1, 1);
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    return validCount;
}

void RadixSortPass::sort(RenderContext& renderContext, unsigned int validCount)
{
    glu::RadixSort sorter;
    sorter(renderContext.keysBuffer, renderContext.valuesBuffer, validCount); //Syncronization is already handled within the sorter
};

void RadixSortPass::gatherPost(RenderContext& renderContext, unsigned int validCount)
{
    glUseProgram(renderContext.shaderPrograms.radixSortGatherProgram);
    setUniform1ui(renderContext.shaderPrograms.radixSortGatherProgram, "u_count", validCount);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.gaussianBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderContext.gaussianBuffer);
    setupSortedBufferSsbo(validCount, renderContext.gaussianBufferSorted, 1); // <-- last uint is binding pos
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.gaussianBufferSorted);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, renderContext.valuesBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderContext.drawIndirectBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, renderContext.drawIndirectBuffer);
    unsigned int threadGroup_xy = (validCount + 255) / 256;
    glDispatchCompute(threadGroup_xy, 1, 1);

    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
}
