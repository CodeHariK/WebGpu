import { BrowserRouter as Router, Routes, Route } from "react-router-dom";
import Dashboard from "./components/Dashboard";
import ImagesPage from "./pages/ImagesPage";
import VolumesPage from "./pages/VolumesPage";
import NetworksPage from "./pages/NetworksPage";
import ContainerDetailsPage from "./pages/ContainerDetailsPage";
import RegistriesPage from "./pages/RegistriesPage";
import SettingsPage from "./pages/SettingsPage";

import { CommandLogProvider } from "./contexts/CommandLogContext";
import CommandLogOverlay from "./components/CommandLogOverlay";

function App() {
  return (
    <CommandLogProvider>
      <Router>
        <Routes>
          <Route path="/" element={<Dashboard />} />
          <Route path="/images" element={<ImagesPage />} />
          <Route path="/volumes" element={<VolumesPage />} />
          <Route path="/networks" element={<NetworksPage />} />
          <Route path="/container/:id" element={<ContainerDetailsPage />} />
          <Route path="/registries" element={<RegistriesPage />} />
          <Route path="/settings" element={<SettingsPage />} />
          {/* We'll add remaining pages later */}
          <Route path="*" element={<Dashboard />} />
        </Routes>
      </Router>
      <CommandLogOverlay />
    </CommandLogProvider>
  );
}

export default App;
