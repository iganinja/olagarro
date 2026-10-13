#include <oui/script/script_engine.h>

#include <gtest/gtest.h>

#include <vector>

using namespace olagarro;

TEST(OuiScriptFloatArrayTests, FloatsGoToAScriptAndComeBackAsAFloat32Array)
{
    oui::script::ScriptEngine engine;
    std::vector<float> back;
    engine.define("halve",
                  [&engine](const std::vector<oui::script::Value>& arguments)
                  {
                      auto values{arguments.at(0).to_floats()};
                      for(auto& value : values)
                      {
                          value /= 2.0f;
                      }
                      return engine.float_array(values.data(), values.size());
                  });
    engine.define("keep",
                  [&back, &engine](const std::vector<oui::script::Value>& arguments)
                  {
                      EXPECT_TRUE(arguments.at(0).is_float_array());
                      back = arguments.at(0).to_floats();
                      return engine.undefined();
                  });
    engine.run("const halved = halve(new Float32Array([2, 4, 0.5]));"
               "if(!(halved instanceof Float32Array)) throw new Error('not a Float32Array');"
               "halved[0] += 10;"
               "keep(halved);",
               "test.js");
    EXPECT_EQ(back, (std::vector<float>{11.0f, 2.0f, 0.25f}));
}

TEST(OuiScriptFloatArrayTests, WhatIsNotAFloat32ArrayIsRefused)
{
    oui::script::ScriptEngine engine;
    engine.define("take",
                  [&engine](const std::vector<oui::script::Value>& arguments)
                  {
                      EXPECT_FALSE(arguments.at(0).is_float_array());
                      EXPECT_THROW(static_cast<void>(arguments.at(0).to_floats()), oui::script::ScriptError);
                      return engine.undefined();
                  });
    engine.run("take([1, 2, 3]); take(new Uint8Array(3));", "test.js");
}
