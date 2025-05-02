add_packages("catch2")

add_deps("cutecoro")

-- pt: performance tests
-- st: sample tests
-- ut: unit tests

includes("st")
includes("ut")
includes("misc")
