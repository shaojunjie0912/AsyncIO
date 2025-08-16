add_packages("catch2")

add_deps("asyncio")

-- pt: performance tests
-- st: sample tests
-- ut: unit tests

includes("st")
includes("ut")
includes("misc")
