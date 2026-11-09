
# PRIVATE, not PUBLIC: nothing links against an executable, so PUBLIC only
# pollutes the target's interface. main.cpp is deliberately absent here — it is
# already passed to add_executable() in the parent CMakeLists.txt, and listing
# it in both places compiles the same translation unit twice.
target_sources(${PROJECT_NAME} PRIVATE
    AP3216_WE.cpp
    helpers_rand.cpp)