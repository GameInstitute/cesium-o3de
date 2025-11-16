#include "Cesium/Systems/HttpAssetAccessor.h"
#include "Cesium/Systems/HttpManager.h"
#include <AzCore/Memory/PoolAllocator.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <CesiumAsync/AsyncSystem.h>

class HttpAssetAccessorTest : public UnitTest::LeakDetectionFixture
{
public:
    void SetUp() override
    {
        UnitTest::LeakDetectionFixture::SetUp();
    }

    void TearDown() override
    {
        UnitTest::LeakDetectionFixture::TearDown();
    }
};

TEST_F(HttpAssetAccessorTest, TestRequestAsset)
{
    // we don't care about worker thread in this test
    CesiumAsync::AsyncSystem asyncSystem{ nullptr };
    Cesium::HttpManager httpManager;

    Cesium::HttpAssetAccessor accessor(&httpManager);
    auto completedRequestFuture = accessor.requestAsset(asyncSystem, "https://httpbin.org/ip");
    auto completedRequest = completedRequestFuture.wait();

    ASSERT_NE(completedRequest, nullptr);
    ASSERT_EQ(completedRequest->response()->statusCode(), 200);
    ASSERT_EQ(completedRequest->method(), "GET");
}

TEST_F(HttpAssetAccessorTest, TestPost)
{
    // we don't care about worker thread in this test
    CesiumAsync::AsyncSystem asyncSystem{ nullptr };
    Cesium::HttpManager httpManager;

    Cesium::HttpAssetAccessor accessor(&httpManager);
    auto completedRequestFuture = accessor.post(asyncSystem, "https://httpbin.org/post");
    auto completedRequest = completedRequestFuture.wait();

    ASSERT_NE(completedRequest, nullptr);
    ASSERT_EQ(completedRequest->response()->statusCode(), 200);
    ASSERT_EQ(completedRequest->method(), "POST");
}
