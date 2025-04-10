target("step1", function()
    set_kind("binary")
    add_files("steps/step1.cpp")
end)

target("step2", function()
    set_kind("binary")
    add_files("steps/step2.cpp")
end)
