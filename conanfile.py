import os
from conans import ConanFile, tools

class NpmMasMas(ConanFile):
    name = "cppunix"
    version = "1.0.0"
    license = "Attribution 4.0 International"
    url = "https://github.com/makiolo/asyncply"
    description = "This fast event system allows calls between two interfaces decoupled (sync or async)"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = {"shared": True}
    generators = "cmake"

    def requirements(self):
        self.requires('fast-event-system/1.0.18@npm-mas-mas/testing')
        self.requires('spdlog/1.3.1@bincrafters/stable')

    def source(self):
        self.run("git clone {}".format(self.url))

    def build(self):
        self.run("cd {} && npm install && npm test".format(self.name))

    def package(self):
        self.copy("{}/*.h".format(self.name), dst="include", excludes=["{}/node_modules".format(self.name), "{}/readerwriterqueue".format(self.name)])
        self.copy("{}/bin/{}/*.lib".format(self.name, self.settings.build_type), dst="lib", keep_path=False)
        self.copy("{}/bin/{}/*.dll".format(self.name, self.settings.build_type), dst="bin", keep_path=False)
        self.copy("{}/bin/{}/*_unittest".format(self.name, self.settings.build_type), dst="unittest", keep_path=False)
        self.copy("{}/bin/{}/*.so".format(self.name, self.settings.build_type), dst="lib", keep_path=False)
        self.copy("{}/bin/{}/*.dylib".format(self.name, self.settings.build_type), dst="lib", keep_path=False)
        self.copy("{}/bin/{}/*.a".format(self.name, self.settings.build_type), dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = [lib for lib in tools.collect_libs(self)]

