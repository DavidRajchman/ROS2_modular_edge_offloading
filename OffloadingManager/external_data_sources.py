"""
External Data Sources for the Offloading Manager Algorithm
Provides HTTP API data collection capabilities for research algorithms.

This module contains the implementation for fetching external data via HTTP APIs,
with caching, background updates, and error handling capabilities.
"""

import logging
import threading
import time
import requests
import os
from datetime import datetime
from typing import Dict, Set, Optional, Any, Union
from dataclasses import dataclass


# External data source configuration constants
# These are duplicated from algorithm.py to avoid circular imports
# The configuration dictionary itself is imported from algorithm module when needed

EXTERNAL_DATA_DEFAULT_TIMEOUT = 5.0  # Default HTTP timeout in seconds
EXTERNAL_DATA_DEFAULT_MAX_CACHE_SIZE = 1000  # Default maximum cached data length
EXTERNAL_DATA_DEFAULT_UPDATE_INTERVAL = 60  # Default auto-update interval in seconds
EXTERNAL_DATA_ENABLE_BACKGROUND_UPDATES = True  # Enable background data updates


@dataclass
class DataSourceStatus:
    """Status and metadata for external data source operations"""
    success: bool
    last_updated: Optional[datetime] = None
    error_message: Optional[str] = None
    data_age_seconds: float = 0.0
    cache_hit: bool = False
    http_status_code: Optional[int] = None
    data_size: int = 0


class DataSourceHandler:
    """
    Universal HTTP API data source handler for external data collection.
    
    Provides caching, background updates, and error handling for external data sources.
    Designed to be used by research algorithms for accessing external information.
    """
    
    def __init__(self, name: str, config: Dict[str, Any]):
        self.name = name
        self.logger = logging.getLogger(f'OM.algorithm.datasource.{name}')
        
        # Configuration
        self.url = config.get('url', '')
        self.max_cache_size = config.get('max_cache_size', EXTERNAL_DATA_DEFAULT_MAX_CACHE_SIZE)
        self.update_interval = config.get('update_interval', EXTERNAL_DATA_DEFAULT_UPDATE_INTERVAL)
        self.timeout_seconds = config.get('timeout_seconds', EXTERNAL_DATA_DEFAULT_TIMEOUT)
        self.auto_update = config.get('auto_update', False)
        
        # State
        self.cached_data: Optional[str] = None
        self.last_updated: Optional[datetime] = None
        self.last_status = DataSourceStatus(success=False)
        self.update_thread: Optional[threading.Thread] = None
        self.stop_auto_update = threading.Event()
        self.data_lock = threading.Lock()
        
        self.logger.info(f"Data source '{name}' initialized: url={self.url}, auto_update={self.auto_update}")
        
        # Start auto-update if enabled
        if self.auto_update and self.url and EXTERNAL_DATA_ENABLE_BACKGROUND_UPDATES:
            self._start_auto_update()
    
    def configure(self, url: str, max_cache_size: int = None, update_interval: int = None, 
                 timeout_seconds: float = None, auto_update: bool = None) -> bool:
        """
        Configure or reconfigure the data source.
        
        Args:
            url: HTTP endpoint URL
            max_cache_size: Maximum length of cached data (None to keep current)
            update_interval: Auto-update interval in seconds (None to keep current)
            timeout_seconds: HTTP request timeout (None to keep current)
            auto_update: Enable/disable auto-update (None to keep current)
            
        Returns:
            bool: True if configuration was successful
        """
        try:
            with self.data_lock:
                # Stop existing auto-update
                if self.update_thread and self.update_thread.is_alive():
                    self.stop_auto_update.set()
                    self.update_thread.join(timeout=2.0)
                
                # Update configuration
                self.url = url
                if max_cache_size is not None:
                    self.max_cache_size = max_cache_size
                if update_interval is not None:
                    self.update_interval = update_interval
                if timeout_seconds is not None:
                    self.timeout_seconds = timeout_seconds
                if auto_update is not None:
                    self.auto_update = auto_update
                
                # Restart auto-update if needed
                self.stop_auto_update.clear()
                if self.auto_update and self.url and EXTERNAL_DATA_ENABLE_BACKGROUND_UPDATES:
                    self._start_auto_update()
                
                self.logger.info(f"Data source '{self.name}' reconfigured: url={self.url}")
                return True
                
        except Exception as e:
            self.logger.error(f"Failed to configure data source '{self.name}': {e}")
            return False
    
    def get_data(self) -> tuple[Optional[str], DataSourceStatus]:
        """
        Get cached data and status.
        
        Returns:
            tuple: (cached_data, status_info)
        """
        with self.data_lock:
            # Update data age
            if self.last_updated:
                age = (datetime.now() - self.last_updated).total_seconds()
                self.last_status.data_age_seconds = age
            
            self.last_status.cache_hit = True
            return self.cached_data, self.last_status
    
    def update_data(self) -> bool:
        """
        Update data in the background (non-blocking).
        
        Returns:
            bool: True if update was initiated successfully
        """
        if not self.url:
            self.logger.warning(f"Cannot update data source '{self.name}': no URL configured")
            return False
        
        try:
            # Start update in background thread
            update_thread = threading.Thread(target=self._fetch_data, name=f"DataUpdate-{self.name}")
            update_thread.daemon = True
            update_thread.start()
            return True
            
        except Exception as e:
            self.logger.error(f"Failed to start background update for '{self.name}': {e}")
            return False
    
    def update_and_get_data(self) -> tuple[Optional[str], DataSourceStatus]:
        """
        Update data synchronously and return result (blocking).
        
        Returns:
            tuple: (updated_data, status_info)
        """
        if not self.url:
            status = DataSourceStatus(
                success=False,
                error_message="No URL configured",
                data_age_seconds=0.0
            )
            return None, status
        
        # Fetch data synchronously
        self._fetch_data()
        
        # Return updated data
        return self.get_data()
    
    def delete(self):
        """
        Clean up resources and stop background updates.
        """
        self.logger.info(f"Deleting data source '{self.name}'")
        
        # Stop auto-update thread
        if self.update_thread and self.update_thread.is_alive():
            self.stop_auto_update.set()
            self.update_thread.join(timeout=2.0)
        
        # Clear cached data
        with self.data_lock:
            self.cached_data = None
            self.last_updated = None
    
    def _start_auto_update(self):
        """Start background auto-update thread."""
        if self.update_thread and self.update_thread.is_alive():
            return
        
        self.update_thread = threading.Thread(target=self._auto_update_loop, name=f"AutoUpdate-{self.name}")
        self.update_thread.daemon = True
        self.update_thread.start()
        self.logger.debug(f"Started auto-update thread for '{self.name}' (interval: {self.update_interval}s)")
    
    def _auto_update_loop(self):
        """Background auto-update loop."""
        while not self.stop_auto_update.is_set():
            try:
                self._fetch_data()
                # Wait for next update or stop signal
                if self.stop_auto_update.wait(timeout=self.update_interval):
                    break  # Stop signal received
            except Exception as e:
                self.logger.error(f"Error in auto-update loop for '{self.name}': {e}")
                # Continue loop despite errors
        
        self.logger.debug(f"Auto-update thread stopped for '{self.name}'")
    
    def _fetch_data(self):
        """Fetch data from HTTP endpoint and update cache."""
        try:
            start_time = time.time()
            response = requests.get(self.url, timeout=self.timeout_seconds)
            fetch_time = time.time() - start_time
            
            # Check for HTTP errors
            response.raise_for_status()
            
            # Get response data
            data = response.text
            
            # Enforce cache size limit
            if len(data) > self.max_cache_size:
                self.logger.warning(f"Data source '{self.name}': truncating data from {len(data)} to {self.max_cache_size} characters")
                data = data[:self.max_cache_size]
            
            # Update cache and status
            with self.data_lock:
                self.cached_data = data
                self.last_updated = datetime.now()
                self.last_status = DataSourceStatus(
                    success=True,
                    last_updated=self.last_updated,
                    error_message=None,
                    data_age_seconds=0.0,
                    cache_hit=False,
                    http_status_code=response.status_code,
                    data_size=len(data)
                )
            
            self.logger.debug(f"Successfully fetched {len(data)} characters from '{self.name}' in {fetch_time:.3f}s")
            
        except requests.exceptions.Timeout:
            error_msg = f"Timeout after {self.timeout_seconds}s"
            self._handle_fetch_error(error_msg, None)
        except requests.exceptions.RequestException as e:
            error_msg = f"HTTP request failed: {str(e)}"
            status_code = getattr(e.response, 'status_code', None) if hasattr(e, 'response') and e.response else None
            self._handle_fetch_error(error_msg, status_code)
        except Exception as e:
            error_msg = f"Unexpected error: {str(e)}"
            self._handle_fetch_error(error_msg, None)
    
    def _handle_fetch_error(self, error_message: str, status_code: Optional[int]):
        """Handle fetch errors and update status."""
        with self.data_lock:
            self.last_status = DataSourceStatus(
                success=False,
                last_updated=self.last_updated,  # Keep previous timestamp
                error_message=error_message,
                data_age_seconds=(datetime.now() - self.last_updated).total_seconds() if self.last_updated else 0.0,
                cache_hit=False,
                http_status_code=status_code,
                data_size=len(self.cached_data) if self.cached_data else 0
            )
        
        self.logger.warning(f"Failed to fetch data from '{self.name}': {error_message}")


class ExternalDataManager:
    """
    Manager for multiple external data sources.
    
    Provides a centralized interface for managing multiple DataSourceHandler instances
    based on the EXTERNAL_DATA_SOURCES configuration.
    """
    
    def __init__(self):
        self.logger = logging.getLogger('OM.algorithm.datamanager')
        self.sources: Dict[str, DataSourceHandler] = {}
        
        # Import configuration to avoid circular imports
        try:
            from algorithm import EXTERNAL_DATA_SOURCES
            configured_sources = EXTERNAL_DATA_SOURCES
        except ImportError:
            self.logger.warning("Could not import EXTERNAL_DATA_SOURCES from algorithm module")
            configured_sources = {}
        
        # Initialize configured data sources
        for name, config in configured_sources.items():
            try:
                self.sources[name] = DataSourceHandler(name, config)
                self.logger.info(f"Initialized data source: {name}")
            except Exception as e:
                self.logger.error(f"Failed to initialize data source '{name}': {e}")
        
        if self.sources:
            self.logger.info(f"External data manager initialized with {len(self.sources)} data sources")
        else:
            self.logger.info("External data manager initialized with no data sources configured")
    
    def get_source(self, name: str) -> Optional[DataSourceHandler]:
        """
        Get a data source handler by name.
        
        Args:
            name: Name of the data source
            
        Returns:
            DataSourceHandler instance or None if not found
        """
        return self.sources.get(name)
    
    def add_source(self, name: str, config: Dict[str, Any]) -> bool:
        """
        Add a new data source at runtime.
        
        Args:
            name: Unique name for the data source
            config: Configuration dictionary with url, max_cache_size, etc.
            
        Returns:
            bool: True if source was added successfully
        """
        if name in self.sources:
            self.logger.warning(f"Data source '{name}' already exists, replacing it")
            self.sources[name].delete()
        
        try:
            self.sources[name] = DataSourceHandler(name, config)
            self.logger.info(f"Added data source: {name}")
            return True
        except Exception as e:
            self.logger.error(f"Failed to add data source '{name}': {e}")
            return False
    
    def remove_source(self, name: str) -> bool:
        """
        Remove and clean up a data source.
        
        Args:
            name: Name of the data source to remove
            
        Returns:
            bool: True if source was removed successfully
        """
        if name not in self.sources:
            self.logger.warning(f"Data source '{name}' not found")
            return False
        
        try:
            self.sources[name].delete()
            del self.sources[name]
            self.logger.info(f"Removed data source: {name}")
            return True
        except Exception as e:
            self.logger.error(f"Failed to remove data source '{name}': {e}")
            return False
    
    def get_all_data(self) -> Dict[str, tuple[Optional[str], DataSourceStatus]]:
        """
        Get data from all configured sources.
        
        Returns:
            Dict mapping source names to (data, status) tuples
        """
        results = {}
        for name, source in self.sources.items():
            try:
                results[name] = source.get_data()
            except Exception as e:
                self.logger.error(f"Error getting data from source '{name}': {e}")
                error_status = DataSourceStatus(
                    success=False,
                    error_message=f"Manager error: {str(e)}"
                )
                results[name] = (None, error_status)
        
        return results
    
    def update_all_sources(self) -> Dict[str, bool]:
        """
        Trigger background updates for all sources.
        
        Returns:
            Dict mapping source names to update success status
        """
        results = {}
        for name, source in self.sources.items():
            try:
                results[name] = source.update_data()
            except Exception as e:
                self.logger.error(f"Error updating source '{name}': {e}")
                results[name] = False
        
        return results
    
    def get_source_names(self) -> Set[str]:
        """
        Get names of all configured data sources.
        
        Returns:
            Set of source names
        """
        return set(self.sources.keys())
    
    def cleanup(self):
        """
        Clean up all data sources and stop background threads.
        """
        self.logger.info("Cleaning up external data manager")
        for name, source in self.sources.items():
            try:
                source.delete()
            except Exception as e:
                self.logger.error(f"Error cleaning up source '{name}': {e}")
        
        self.sources.clear()
        self.logger.info("External data manager cleanup complete")