/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/enqueue_barrier.h"
#include "opencl/source/command_queue/enqueue_marker.h"
#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct GetSizeRequiredTest : public CommandEnqueueFixture,
                             public ::testing::Test {

    void SetUp() override {
        CommandEnqueueFixture::setUp();
        dsh = &pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
        ioh = &pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
        ssh = &pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);

        usedBeforeDSH = dsh->getUsed();
        usedBeforeIOH = ioh->getUsed();
        usedBeforeSSH = ssh->getUsed();
    }

    void TearDown() override {
        CommandEnqueueFixture::tearDown();
    }

    IndirectHeap *dsh;
    IndirectHeap *ioh;
    IndirectHeap *ssh;

    size_t usedBeforeDSH;
    size_t usedBeforeIOH;
    size_t usedBeforeSSH;
};

HWTEST_F(GetSizeRequiredTest, WhenFinishingThenHeapsAndCommandBufferAreNotConsumed) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();

    auto retVal = pCmdQ->finish();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, commandStream.getUsed() - usedBeforeCS);
    EXPECT_EQ(0u, dsh->getUsed() - usedBeforeDSH);
    EXPECT_EQ(0u, ioh->getUsed() - usedBeforeIOH);
    EXPECT_EQ(0u, ssh->getUsed() - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredTest, WhenEnqueuingMarkerThenHeapsAndCommandBufferAreNotConsumed) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();

    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    cl_event eventBeingWaitedOn = &event1;
    cl_event eventReturned = nullptr;
    auto retVal = pCmdQ->enqueueMarkerWithWaitList(
        1,
        &eventBeingWaitedOn,
        &eventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t expectedStreamSize = 0;
    if (pCmdQ->getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        expectedStreamSize = alignUp(MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(
                                         pDevice->getRootDeviceEnvironment(), false),
                                     MemoryConstants::cacheLineSize);
    }
    EXPECT_EQ(expectedStreamSize, commandStream.getUsed() - usedBeforeCS);
    EXPECT_EQ(0u, dsh->getUsed() - usedBeforeDSH);
    EXPECT_EQ(0u, ioh->getUsed() - usedBeforeIOH);
    EXPECT_EQ(0u, ssh->getUsed() - usedBeforeSSH);

    clReleaseEvent(eventReturned);
}

HWTEST_F(GetSizeRequiredTest, WhenEnqueuingBarrierThenHeapsAndCommandBufferAreNotConsumed) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();

    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    cl_event eventBeingWaitedOn = &event1;
    cl_event eventReturned = nullptr;
    auto retVal = pCmdQ->enqueueBarrierWithWaitList(
        1,
        &eventBeingWaitedOn,
        &eventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t expectedStreamSize = 0;
    if (pCmdQ->getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        auto unalignedSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(pDevice->getRootDeviceEnvironment(), false) +
                             EncodeStoreMemory<FamilyType>::getStoreDataImmSize() +
                             sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
        expectedStreamSize = alignUp(unalignedSize, MemoryConstants::cacheLineSize);
    }

    EXPECT_EQ(expectedStreamSize, commandStream.getUsed() - usedBeforeCS);

    clReleaseEvent(eventReturned);
}
