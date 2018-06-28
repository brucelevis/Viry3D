/*
* Viry3D
* Copyright 2014-2018 by Stack - stackos@qq.com
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "string/String.h"
#include <functional>

namespace Viry3D
{
    typedef std::function<void()> Event;

    class ThreadPool;
    class ApplicationPrivate;

    class Application
    {
    public:
        static void SetName(const String& name);
        static const String& Name();
        static const String& DataPath();
        static const String& SavePath();
        static ThreadPool* ThreadPool();
        static void PostEvent(Event event);
        static void ProcessEvents();
        static void ClearEvents();
        static void UpdateBegin();
        static void UpdateEnd();
        Application();
        virtual ~Application();
        virtual void Update() { }

    private:
        ApplicationPrivate* m_private;
    };
}
