# ParameterMapper

This is a simple JUCE module which lets you add cc-to-parameter mapping in a JUCE plugin. 

Simply make an instance of `ParameterMappingManager` in your Processor, add a call to `ParameterMappingManager::Process(MidiBuffer&)` in your ProcessBlock, and map a cc to a parameter with `ParameterMappingManager::addParameterMapping`. 


